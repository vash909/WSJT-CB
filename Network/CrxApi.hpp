#ifndef CRX_API_HPP_
#define CRX_API_HPP_

#include <boost/core/noncopyable.hpp>
#include <QObject>
#include "Radio.hpp"
#include "pimpl_h.hpp"

class Configuration;
class QNetworkAccessManager;

//
// CrxApi — CRX Cloud API client for WSJT-CB
//
// Sends logged QSOs to a CRX logbook and optional DX spots to the DXCluster
// via the single POST endpoint at https://s.crx.cloud/api/
//
class CrxApi final
  : public QObject
{
  Q_OBJECT

public:
  using Frequency = Radio::Frequency;

  struct LogbookEntry
  {
    int log_id = 0;
    QString log_name;
  };

  explicit CrxApi (Configuration const *, QNetworkAccessManager *, QObject * parent = nullptr);
  ~CrxApi ();

  // Send a logged QSO (ADIF) to the configured CRX logbook
  void logQso (QByteArray const& ADIF);

  // Send a DX spot to the CRX DXCluster
  //void sendSpot (QString const& dx_call, QString const& dx_grid,
  //               Frequency frequency, QString const& mode, int snr);
  void sendSpot (QString const& dx_call, QString const& dx_grid, Frequency frequency, QString const& mode, int snr_rx, int snr_snd);

  // Test the API key (health_check)
  Q_SLOT void testApi ();

  // Fetch the user's logbooks from CRX
  Q_SLOT void fetchLogs ();

  Q_SIGNAL void apikey_ok () const;
  Q_SIGNAL void apikey_ro () const;
  Q_SIGNAL void apikey_invalid () const;

  // Emitted when logbook list is retrieved
  Q_SIGNAL void logs_ready (QList<LogbookEntry>) const;

private:
  class impl;
  pimpl<impl> m_;
};

#endif
