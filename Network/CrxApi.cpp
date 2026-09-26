#include "CrxApi.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPointer>
#include <QUrl>
#include <QDateTime>
#include <QRegularExpression>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QIODevice>

#include "pimpl_impl.hpp"

#include "moc_CrxApi.cpp"

#include "Configuration.hpp"

using Frequency = Radio::Frequency;

class CrxApi::impl final
  : public QObject
{
  Q_OBJECT

public:
  impl (CrxApi * self, Configuration const * config, QNetworkAccessManager * network_manager)
    : self_ {self}
    , config_ {config}
    , network_manager_ {network_manager}
  {
  }

  //  Internal helpers 

  QJsonObject buildRequest (QString const& query, QJsonObject const& extra = {})
  {
    QJsonObject req;
    req.insert ("type", "radio");
    req.insert ("query", query);
    req.insert ("apikey", config_->crx_api_key ());
    if (!extra.isEmpty ()) req.insert ("req", extra);
    return req;
  }

  QByteArray buildBody (QString const& query, QJsonObject const& extra = {})
  {
    QJsonObject body;
    QJsonObject req;
    req.insert ("type", "radio");
    req.insert ("query", query);
    req.insert ("apikey", config_->crx_api_key ());
    if (!extra.isEmpty ())
    {
      // merge extra into req
      for (auto it = extra.constBegin (); it != extra.constEnd (); ++it)
        req.insert (it.key (), it.value ());
    }
    body.insert ("req", req);
    return QJsonDocument {body}.toJson (QJsonDocument::Compact);
  }

  QUrl apiUrl () const
  {
    return QUrl {QStringLiteral ("https://s.crx.cloud/api/")};
  }

  //  ADIF parser 

  // Parse an ADIF string into a flat map (key -> value), all keys lower-cased.
  QHash<QString, QString> parseAdif (QString const& adif)
  {
    QHash<QString, QString> fields;
    // Strip <eor> if present
    QString text = adif.trimmed ();
    if (text.toLower ().endsWith ("<eor>"))
      text.chop (5);

    // Find field definitions (optional, e.g. <EOH>...<EOH>)
    QHash<QString, int> fieldLengths;
    QRegularExpression eohRe {"<EOH>(.*?)</EOH>", QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption};
    auto eit = eohRe.globalMatch (text);
    while (eit.hasNext ())
    {
      auto eoh = eit.next ();
      auto line = eoh.captured (1).trimmed ();
      auto space = line.indexOf (' ');
      if (space > 0 && space < line.length () - 1)
      {
        QString key = line.left (space).trimmed ().toLower ();
        int len = line.mid (space + 1).trimmed ().toInt ();
        fieldLengths[key] = len;
      }
    }
    // Remove EOH section from text for parsing
    text = text.replace (eohRe, "");

    // Parse fields: <key:value>value or <key>value
    QRegularExpression fieldRe {"<([^:/>]+)(?::(\\d+))?>([^<]*)"};
    auto fit = fieldRe.globalMatch (text);
    while (fit.hasNext ())
    {
      auto m = fit.next ();
      QString key = m.captured (1).toLower ();
      QString value = m.captured (3);
      int specifiedLen = m.captured (2).toInt ();
      if (specifiedLen > 0 && static_cast<int> (value.length ()) > specifiedLen)
        value = value.left (specifiedLen);
      else if (fieldLengths.contains (key) && static_cast<int> (value.length ()) > fieldLengths[key])
        value = value.left (fieldLengths[key]);
      fields[key] = value.trimmed ();
    }
    return fields;
  }

  QString hzToBand (int freq_hz) const
  {
    double mhz = freq_hz / 1e6;
    // Bands matching wsjtx_crx_bridge.py BAND_MAP
    if (mhz >= 0.1355 && mhz <= 0.1385)  return "136khz";
    if (mhz >= 0.4720 && mhz <= 0.4790)  return "500khz";
    if (mhz >= 1.800  && mhz <= 2.000)   return "160m";
    if (mhz >= 3.500  && mhz <= 4.000)   return "80m";
    if (mhz >= 5.250  && mhz <= 5.450)   return "60m";
    if (mhz >= 7.000  && mhz <= 7.300)   return "40m";
    if (mhz >= 10.100 && mhz <= 10.150)  return "30m";
    if (mhz >= 14.000 && mhz <= 14.350)  return "20m";
    if (mhz >= 18.068 && mhz <= 18.168)  return "17m";
    if (mhz >= 21.000 && mhz <= 21.450)  return "15m";
    if (mhz >= 24.890 && mhz <= 24.990)  return "12m";
    if (mhz >= 26.000 && mhz <= 27.999)  return "11m";
    if (mhz >= 28.000 && mhz <= 29.700)  return "10m";
    if (mhz >= 50.000 && mhz <= 54.000)  return "6m";
    if (mhz >= 144.000 && mhz <= 148.000) return "2m";
    return "???";
  }

  QString hzToKhz (int freq_hz) const
  {
    //return QString::number (freq_hz / 1000.0, 'f', 3);
    QString s = QString::number (freq_hz / 1000.0, 'f', 3);
    while (s.endsWith ('0')) s.chop (1);
    if (s.endsWith ('.')) s.chop (1);
    return s;
  }

  //  Public methods 

	void debugLog (QString const& line)
	{
		QFile f {QDir::tempPath () + "/crxapi_debug.log"};
		if (f.open (QIODevice::Append | QIODevice::Text))
		  QTextStream {&f} << QDateTime::currentDateTimeUtc ().toString (Qt::ISODate) << ' ' << line << '\n';
		else
		  qWarning() << "CrxApi debugLog: cannot open" << f.fileName() << f.errorString();
	}

  void logQso (QByteArray const& ADIF)
  {
    auto fields = parseAdif (QString::fromUtf8 (ADIF));

    //?QString my_call = fields.value ("mycall", "").toUpper ();
    QString dx_call = fields.value ("call", "").toUpper ();
    QString mode = fields.value ("mode", "").toUpper ();
    QString submode = fields.value ("submode", "").toUpper ();
    if (!submode.isEmpty ())
      mode = submode;             // FT4, Q65... sont loggs MFSK + submode
    QString band = fields.value ("band", "").toLower ();
    QString freq_str = fields.value ("freq", fields.value ("qso_freq", ""));
	
	QString rst_sent = fields.value ("rst_sent", "599");
    QString rst_rcvd = fields.value ("rst_rcvd", "599");
	
    QString dx_grid = fields.value ("gridsquare", "").toUpper ();
    QString name = fields.value ("name", "");
    QString comment = fields.value ("comment", "");

    // Derive freq_khz and band from qso_freq if not present
    //double freq_mhz = freq_str.toDouble ();
    //int freq_hz = static_cast<int> (freq_mhz * 1e6);

    double freq_mhz = freq_str.toDouble ();
    int freq_hz = qRound (freq_mhz * 1e6);	

    if (band.isEmpty () || band == "???")
      band = hzToBand (freq_hz);
    QString freq_khz = hzToKhz (freq_hz);

    // Build comment with WSJT-CB marker
    if (!comment.isEmpty ())
      comment = comment + " | WSJTX-CRX";
    else
      comment = "WSJTX-CRX";

    QJsonObject qsoData;
    qsoData.insert ("qso_id", 0);
    qsoData.insert ("f_log_id", config_->crx_logbook_id ());
    qsoData.insert ("logentry_his_call", dx_call);
    qsoData.insert ("logentry_his_name", name);
	qsoData.insert ("logentry_his_locator", dx_grid);
    qsoData.insert ("logentry_band", band);
    qsoData.insert ("logentry_frequency", freq_khz);
    qsoData.insert ("logentry_mode", mode);
    qsoData.insert ("logentry_his_report", rst_sent);
    qsoData.insert ("logentry_my_report", rst_rcvd);
    qsoData.insert ("logentry_comment", comment);
	
	//vqsl status   2=> is sent,  default for DIGIT QSO. 
	qsoData.insert ("logentry_custom_field23","2");
	
    QJsonObject extra;
    extra.insert ("qsoData", qsoData);

    QByteArray data = buildBody ("edit_myqso", extra);
    sendPost (data, &CrxApi::impl::reply_logqso);
  }

  void sendSpot (QString const& dx_call, QString const& dx_grid, Frequency frequency, QString const& mode, int snr_rx, int snr_snd)
  {

    debugLog (QString ("sendSpot dx=%1 freq_hz=%2 mode=%3 snr=%4")
              .arg (dx_call).arg (frequency).arg (mode).arg (snr_rx));

    QString my_call = config_->my_callsign ().toUpper ();
    
	QString freq_khz = QString::number (qRound (static_cast<double> (frequency) / 1000.0));

	//exemple comment : FT8  Sent: -07  Rcvd: -16
    QString contact_comment = QString ("%1 Sent: %2  Rcvd: %3").arg (mode).arg (snr_rx).arg (snr_snd);

    QJsonObject spotData;
    spotData.insert ("locator_dx", dx_grid.toUpper ());
	spotData.insert ("callsign_dx", dx_call.toUpper ());
	spotData.insert ("callsign_sender", my_call);
	spotData.insert ("select_report_part1_form", snr_rx);
	spotData.insert ("frequency", freq_khz);
	spotData.insert ("mode", mode.toUpper ());
	spotData.insert ("type", "WRK");
	spotData.insert ("contact_comment", contact_comment);

    QJsonObject extra;
    extra.insert ("spotData", spotData);

    QByteArray data = buildBody ("edit_myspot", extra);
    sendPost (data, &CrxApi::impl::reply_sendspot);
  }

  void testApi ()
  {
    QByteArray data = buildBody ("health_check");
    sendPost (data, &CrxApi::impl::reply_apitest);
  }

  void fetchLogs ()
  {
    QByteArray data = buildBody ("get_mylogs");
    sendPost (data, &CrxApi::impl::reply_fetchlogs);
  }

private:
  //  HTTP helpers 

  using ReplyHandler = void (CrxApi::impl::*)(QPointer<QNetworkReply>);

  void sendPost (QByteArray const& data, ReplyHandler handler)
  {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    if (QNetworkAccessManager::Accessible != network_manager_->networkAccessible ())
      network_manager_->setNetworkAccessible (QNetworkAccessManager::Accessible);
#endif

    QNetworkRequest request {apiUrl ()};
    request.setHeader (QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader ("User-Agent", "WSJT-CB CRX Bridge");
    request.setAttribute (QNetworkRequest::FollowRedirectsAttribute, true);

    auto * reply = network_manager_->post (request, data);
    QPointer<QNetworkReply> guard {reply};
    connect (reply, &QNetworkReply::finished, this,
             [this, handler, guard] ()
             {
               if (guard)
                 {
                   (this->*handler) (guard);
                   guard->deleteLater ();
                 }
             });
  }

  //  Reply handlers 

  void reply_apitest (QPointer<QNetworkReply> reply)
  {
    if (!reply || reply->error () != QNetworkReply::NoError)
    {
      Q_EMIT self_->apikey_invalid ();
      return;
    }
    auto doc = QJsonDocument::fromJson (reply->readAll ());
    auto obj = doc.object ();
    if (obj.value ("status").toString () == "online")
      Q_EMIT self_->apikey_ok ();
    else
      Q_EMIT self_->apikey_invalid ();
  }

  void reply_logqso (QPointer<QNetworkReply> reply)
  {
    if (!reply || reply->error () != QNetworkReply::NoError)
      return; // silent  user can check status in WSJT-CB log if needed

    auto doc = QJsonDocument::fromJson (reply->readAll ());
    auto obj = doc.object ();
    if (obj.contains ("error"))
    {
      qWarning () << "CrxApi logQso error:" << obj["error"].toString ();
    }
  }

  /*void reply_sendspot (QPointer<QNetworkReply> reply)
  {
    if (!reply || reply->error () != QNetworkReply::NoError)
      return;

    auto doc = QJsonDocument::fromJson (reply->readAll ());
    auto obj = doc.object ();
    if (obj.contains ("error"))
    {
      qWarning () << "CrxApi sendSpot error:" << obj["error"].toString ();
    }
  }*/

  void reply_sendspot (QPointer<QNetworkReply> reply)
  {
    if (!reply) return;
    auto body = QString::fromUtf8 (reply->readAll ());
    debugLog (QString ("spot reply http=%1 net=%2 body=%3")
              .arg (reply->attribute (QNetworkRequest::HttpStatusCodeAttribute).toInt ())
              .arg (reply->errorString ())
              .arg (body));
  }  

  void reply_fetchlogs (QPointer<QNetworkReply> reply)
  {
    if (!reply || reply->error () != QNetworkReply::NoError)
      return;

    auto doc = QJsonDocument::fromJson (reply->readAll ());
    auto obj = doc.object ();
    if (obj.contains ("error"))
    {
      qWarning () << "CrxApi fetchLogs error:" << obj["error"].toString ();
      return;
    }

    auto logsArray = obj.value ("logs").toArray ();
    QList<CrxApi::LogbookEntry> logs;
    for (auto it = logsArray.begin (); it != logsArray.end (); ++it)
    {
      auto l = it->toObject ();
      CrxApi::LogbookEntry entry;
      entry.log_id = l["log_id"].toInt ();
      entry.log_name = l["log_name"].toString ();
      logs.append (entry);
    }
    Q_EMIT self_->logs_ready (logs);
  }

  //  Members 

  CrxApi * self_;
  Configuration const * config_;
  QNetworkAccessManager * network_manager_;
  QPointer<QNetworkReply> reply_;
};

#include "CrxApi.moc"

//  Public API 

CrxApi::CrxApi (Configuration const * config, QNetworkAccessManager * network_manager, QObject * parent)
  : QObject {parent}
  , m_ {this, config, network_manager}
{
}

CrxApi::~CrxApi ()
{
}

void CrxApi::logQso (QByteArray const& ADIF)
{
  m_->logQso (ADIF);
}

void CrxApi::sendSpot (QString const& dx_call, QString const& dx_grid,
                       Frequency frequency, QString const& mode,
                       int snr_rx, int snr_snd)
{
  m_->sendSpot (dx_call, dx_grid, frequency, mode, snr_rx, snr_snd);
}

void CrxApi::testApi ()
{
  m_->testApi ();
}

void CrxApi::fetchLogs ()
{
  m_->fetchLogs ();
}
