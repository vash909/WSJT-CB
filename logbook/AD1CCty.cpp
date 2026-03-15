#include "AD1CCty.hpp"

#include <string>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lexical_cast.hpp>
#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDebugStateSaver>
#include <QRegularExpression>
#include "Configuration.hpp"
#include "Radio.hpp"
#include "pimpl_impl.hpp"
#include "Logger.hpp"

#include "moc_AD1CCty.cpp"

using namespace boost::multi_index;

namespace
{
  auto const file_name = "cty.dat";
  auto const grid_file_name = "grid.dat";    // NJ0A
//  auto const logFileName = "wsjtx_log.adi";  // NJ0A

  std::map<QString, QString> const& cb_NNN_to_country ()
  {
    static std::map<QString, QString> const m {
      {"001", "Italy"}, {"002", "United States Of America"},
      {"003", "Brazil"}, {"004", "Argentina"},
      {"005", "Venezuela"}, {"006", "Colombia"},
      {"008", "Peru"}, {"009", "Canada"},
      {"010", "Mexico"}, {"011", "Puerto Rico"},
      {"012", "Uruguay"}, {"013", "Germany"},
      {"014", "France"}, {"015", "Switzerland"},
      {"016", "Belgium"}, {"017", "Hawaiian Islands"},
      {"018", "Greece"}, {"019", "Netherlands"},
      {"020", "Norway"}, {"021", "Sweden"},
      {"022", "French Guyana"}, {"023", "Jamaica"},
      {"024", "Panama"}, {"025", "Japan"},
      {"026", "England"}, {"027", "Iceland"},
      {"028", "Honduras"}, {"029", "Ireland"},
      {"030", "Spain"}, {"031", "Portugal"},
      {"032", "Chile"}, {"033", "Alaska"},
      {"034", "Canary Islands"}, {"035", "Austria"},
      {"036", "San Marino"}, {"037", "Dominican Republic"},
      {"038", "Greenland"}, {"039", "Angola"},
      {"040", "Liechtenstein"}, {"041", "New Zealand"},
      {"042", "Liberia"}, {"043", "Australia"},
      {"044", "South Africa"}, {"045", "Serbia"},
      {"046", "East Germany"}, {"047", "Denmark"},
      {"048", "Saudi Arabia"}, {"049", "Balearic Islands"},
      {"050", "European Russia"}, {"051", "Andorra"},
      {"052", "Faroe Islands"}, {"053", "El Salvador"},
      {"054", "Luxembourg"}, {"055", "Gibraltar"},
      {"056", "Finland"}, {"057", "India"},
      {"058", "East Malaysia"}, {"059", "Dodecanese Islands"},
      {"060", "Hong Kong"}, {"061", "Ecuador"},
      {"062", "Guam Island"}, {"063", "St. Helena Island"},
      {"064", "Senegal"}, {"065", "Sierra Leone"},
      {"066", "Mauritania"}, {"067", "Paraguay"},
      {"068", "Northern Ireland"}, {"069", "Costa Rica"},
      {"070", "American Samoa Islands"}, {"071", "Midway Islands"},
      {"072", "Guatemala"}, {"073", "Suriname"},
      {"074", "Namibia"}, {"075", "Azores Islands"},
      {"076", "Morocco"}, {"077", "Ghana"},
      {"078", "Zambia"}, {"079", "Philippine Islands"},
      {"080", "Bolivia"}, {"081", "San Andres Providencia"},
      {"082", "Guantanamo Bay"}, {"083", "Tanzania"},
      {"084", "Ivory Coast"}, {"085", "Zimbabwe"},
      {"086", "Nepal"}, {"087", "Yemen"},
      {"088", "Cuba"}, {"089", "Nigeria"},
      {"090", "Crete Island"}, {"091", "Indonesia"},
      {"092", "Libya"}, {"093", "Malta"},
      {"094", "United Arab Emirates"}, {"095", "Mongolia"},
      {"096", "Tonga Islands"}, {"097", "Israel"},
      {"098", "Singapore"}, {"099", "Fiji Islands"},
      {"100", "Korea"}, {"101", "Papua – New Guinea"},
      {"102", "Kuwait"}, {"103", "Haiti"},
      {"104", "Corsica"}, {"105", "Botswana"},
      {"106", "Ceuta & Melilla"}, {"107", "Monaco"},
      {"108", "Scotland"}, {"109", "Hungary"},
      {"110", "Cyprus"}, {"111", "Jordan"},
      {"112", "Lebanon"}, {"113", "West Malaysia"},
      {"114", "Pakistan"}, {"115", "Qatar"},
      {"116", "Turkey"}, {"117", "Egypt"},
      {"118", "The Gambia"}, {"119", "Madeira Island"},
      {"120", "Antigua & Barbuda Isl"}, {"121", "The Bahamas"},
      {"122", "Barbados Island"}, {"123", "Bermuda Island"},
      {"124", "Amsterdam & St. Paul Isl"}, {"125", "Cayman Islands"},
      {"126", "Nicaragua"}, {"127", "Virgin Islands"},
      {"128", "British Virgin Isl."}, {"129", "Macquarie Islands"},
      {"130", "Norfolk Islands"}, {"131", "Guyana"},
      {"132", "Marshall Islands"}, {"133", "Marianas Islands"},
      {"134", "Republic Of Palau"}, {"135", "Solomon Islands"},
      {"136", "Martinique Island"}, {"137", "Isle Of Man"},
      {"138", "Vatican City State"}, {"139", "Southern Yemen"},
      {"140", "Antarctica"}, {"141", "St. Pierre & Miquelon"},
      {"142", "Lesotho"}, {"143", "St. Lucia Island"},
      {"144", "Easter Island"}, {"145", "Galapagos Islands"},
      {"146", "Algeria"}, {"147", "Tunisia"},
      {"148", "Ascension Island"}, {"149", "Laccadive Islands"},
      {"150", "Bahrain"}, {"151", "Iraq"},
      {"152", "Maldives Islands"}, {"153", "Thailand"},
      {"154", "Iran"}, {"155", "Taiwan"},
      {"156", "Cameroon"}, {"157", "Montserrat Island"},
      {"158", "Trinidad & Tobago Isl"}, {"159", "Somali Republic"},
      {"160", "Sudan"}, {"161", "Poland"},
      {"162", "Democratic Republic Of Congo"}, {"163", "Wales"},
      {"164", "Togo Republic"}, {"165", "Sardegna"},
      {"166", "St. Maarten, Saba & St.Eustatius"}, {"167", "Jersey Island"},
      {"168", "Mauritius Islands"}, {"169", "Guernsey Island"},
      {"170", "Burkina Faso"}, {"171", "Svalbard Islands"},
      {"172", "New Caledonia"}, {"173", "Reunion Island"},
      {"174", "Uganda"}, {"175", "Chad Republic"},
      {"176", "Central African Republic"}, {"177", "Sri Lanka"},
      {"178", "Bulgaria"}, {"179", "Czechoslovakia"},
      {"180", "Oman"}, {"181", "Syria"},
      {"182", "Republic Of Guinea"}, {"183", "Benin"},
      {"184", "Burundi"}, {"185", "Comoros Islands"},
      {"186", "Djibouti"}, {"187", "Kenya"},
      {"188", "Malagasy Republic"}, {"189", "Mayotte Island"},
      {"190", "Seychelles Islands"}, {"191", "Kingdom of Eswatini"},
      {"192", "Cocos Islands"}, {"193", "Keeling Islands"},
      {"194", "Dominica Island"}, {"195", "Grenada Island"},
      {"196", "Guadeloupe Islands"}, {"197", "Vanuatu Islands"},
      {"198", "Falkland Islands"}, {"199", "Equatorial Guinea"},
      {"200", "South Shetland Islands"}, {"201", "French Polynesia"},
      {"202", "Bhutan"}, {"203", "China"},
      {"204", "Mozambique"}, {"205", "Republic Of Cape Verde"},
      {"206", "Ethiopia"}, {"207", "St. Martin Island"},
      {"208", "Glorieuses Islands"}, {"209", "Juan De Nova Island"},
      {"210", "Wallis & Futuna Islands"}, {"211", "Jan Mayen Island"},
      {"212", "Aland Islands"}, {"213", "Market Reef"},
      {"214", "Congo Republic"}, {"215", "Gabon Republic"},
      {"216", "Mali"}, {"217", "Christmas Island"},
      {"218", "Belize"}, {"219", "Anguilla Island"},
      {"220", "St. Vincent Island"}, {"221", "South Orkney Islands"},
      {"222", "South Sandwich Islands"}, {"223", "Western Samoa Islands"},
      {"224", "Western Kiribati"}, {"225", "Brunei"},
      {"226", "Malawi"}, {"227", "Rwanda"},
      {"228", "Chagos Islands"}, {"229", "Heard Island"},
      {"230", "Federated States Of Micronesia"}, {"231", "St. Peter & St. Paul Rock"},
      {"232", "Aruba Island"}, {"233", "Romania"},
      {"234", "Afghanistan"}, {"235", "Geneva"},
      {"236", "Bangladesh"}, {"237", "Union Of Myanmar"},
      {"238", "Cambodia"}, {"239", "Laos"},
      {"240", "Macao"}, {"241", "Spratly Island"},
      {"242", "Vietnam"}, {"243", "Gleam & St.Brandon Isl"},
      {"244", "Pagalu Island"}, {"245", "Niger Republic"},
      {"246", "Sao Tome & Principe Isl"}, {"247", "Navassa Island"},
      {"248", "Turks & Caicos Islands"}, {"249", "Northern Cook Islands"},
      {"250", "Cook Islands"}, {"251", "Albania"},
      {"252", "Revillagigedo Islands"}, {"253", "Andaman & Nicobar Island"},
      {"254", "Mount Athos"}, {"255", "Kerguelen Islands"},
      {"256", "Prince Edward & Marion Islands"}, {"257", "Rodriguez Island"},
      {"258", "Tristan Da Cunha & Gough"}, {"259", "Tromelin Island"},
      {"260", "Baker & Howland Islands"}, {"261", "Chatham Islands"},
      {"262", "Johnston Island"}, {"263", "Kermadec Islands"},
      {"264", "Kingman Reef"}, {"265", "Central Kiribati"},
      {"266", "Eastern Kiribati"}, {"267", "Kure Island"},
      {"268", "Lord Howe Islands"}, {"269", "Mellish Reef"},
      {"270", "Minami Torishima Island"}, {"271", "Republic Of Nauru"},
      {"272", "Niue Island"}, {"273", "Jarvis & Palmyra Islands"},
      {"274", "Pitcairn Island"}, {"275", "Tokelau Islands"},
      {"276", "Tuvalu Islands"}, {"277", "Sable Island"},
      {"278", "Wake Island"}, {"279", "Willis Islets"},
      {"280", "Aves Island"}, {"281", "Ogasawara Islands"},
      {"282", "Auckland & Campbell Islands"}, {"283", "St. Kitts & Nevis Island"},
      {"284", "St. Paul Island"}, {"285", "Fernando De Noronha Islands"},
      {"286", "Juan Fernandez Islands"}, {"287", "Malpelo Island"},
      {"288", "San Felix & San Ambrosio"}, {"289", "South Georgia Islands"},
      {"290", "Trindade & Martim Vaz Islands"}, {"291", "Dhekelia & Akrotiri"},
      {"292", "Abu-ail & Jabal-al-tair"}, {"293", "Guinea Bissau"},
      {"294", "Peter 1st Island"}, {"296", "Clipperton Island"},
      {"297", "Bouvet Island"}, {"298", "Crozet Islands"},
      {"299", "Desecheo Island"}, {"300", "West Sahara-rio De Oro"},
      {"301", "Armenia"}, {"302", "Asiatic Russia"},
      {"303", "Azerbaijan"}, {"304", "Estonia"},
      {"305", "Franz Josef Land"}, {"306", "Georgia"},
      {"307", "Kaliningradsk"}, {"308", "Kazakh"},
      {"309", "Kyrgyzstan"}, {"310", "Latvia"},
      {"311", "Lithuania"}, {"312", "Moldavia"},
      {"313", "Tajikistan"}, {"314", "Turkoman"},
      {"315", "Ukraine"}, {"316", "Uzbek"},
      {"317", "Belarus"}, {"318", "Survey Military Of Malta"},
      {"319", "United Nations New York"}, {"320", "Banaba Island"},
      {"321", "Conway Reef"}, {"322", "Walvis Bay"},
      {"323", "Yemen Republic"}, {"324", "Penguin Islands"},
      {"325", "Rotuma Island"}, {"326", "Malyj Vytsotskj"},
      {"327", "Slovenia"}, {"328", "Croatia"},
      {"329", "Czech Republic"}, {"330", "Slovak Republic"},
      {"331", "Bosnia"}, {"332", "North Macedonia"},
      {"333", "Eritrea"}, {"334", "North Korea"},
      {"335", "Scarborough Reef"}, {"336", "Pratas Island"},
      {"337", "Austral Islands"}, {"338", "Marquesas Islands"},
      {"339", "Temotu"}, {"340", "Palestina"},
      {"341", "East Timor"}, {"342", "Chesterfields Islands"},
      {"343", "Ducie Island"}, {"344", "Republic Of Montenegro"},
      {"345", "Swains Island"}, {"346", "St. Barthelemy Island"},
      {"347", "Curacao"}, {"348", "Sint Maarten"},
      {"349", "Saba & Sint-Eustatius"}, {"350", "Bonaire"},
      {"351", "South Sudan"}, {"352", "Republic of Kosovo"}
    };
    return m;
  }

  QString cb_country_name_from_call (QString const& call)
  {
    auto call_base = Radio::base_callsign (call).toUpper ();
    QRegularExpression cb_re {R"(^([0-9]{3})[A-Z]{2}[0-9]{3}$)"};
    auto match = cb_re.match (call_base);
    if (!match.hasMatch ())
      {
        return {};
      }
    auto prefix = match.captured (1);
    auto const& map = cb_NNN_to_country ();
    auto it = map.find (prefix);
    if (it != map.end ())
      {
        return it->second;
      }
    return QString {"CB " + prefix};
  }
}

struct entity
{
  using Continent = AD1CCty::Continent;

  explicit entity (int id
                   , QString const& name
                   , bool WAE_only
                   , int CQ_zone
                   , int ITU_zone
                   , Continent continent
                   , float latitude
                   , float longtitude
                   , int UTC_offset
                   , QString const& primary_prefix)
    : id_ {id}
    , name_ {name}
    , WAE_only_ {WAE_only}
    , CQ_zone_ {CQ_zone}
    , ITU_zone_ {ITU_zone}
    , continent_ {continent}
    , lat_ {latitude}
    , long_ {longtitude}
    , UTC_offset_ {UTC_offset}
    , primary_prefix_ {primary_prefix}
  {
  }

  int id_;
  QString name_;
  bool WAE_only_;               // DARC WAE only, not valid for ARRL awards
  int CQ_zone_;
  int ITU_zone_;
  Continent continent_;
  float lat_;                   // degrees + is North
  float long_;                  // degrees + is West
  int UTC_offset_;              // seconds
  QString primary_prefix_;
};

#define maxPrefix 25        //NJ0A
#define maxIndex 100        //NJ0A

QString gridPrefix [maxPrefix];        //NJ0A
QString gridState [maxPrefix] [maxIndex];   //NJ0A
int gridNumPrefix;          //NJ0A

#if !defined (QT_NO_DEBUG_STREAM)
QDebug operator << (QDebug dbg, entity const& e)
{
  QDebugStateSaver saver {dbg};
  dbg.nospace () << "entity("
                 << e.id_ << ", "
                 << e.name_ << ", "
                 << e.WAE_only_ << ", "
                 << e.CQ_zone_ << ", "
                 << e.ITU_zone_ << ", "
                 << e.continent_ << ", "
                 << e.lat_ << ", "
                 << e.long_ << ", "
                 << (e.UTC_offset_ / (60. * 60.)) << ", "
                 << e.primary_prefix_ << ')';
  return dbg;
}
#endif

// tags
struct id {};
struct primary_prefix {};

// hash operation for QString object instances
struct hash_QString
{
  std::size_t operator () (QString const& qs) const
  {
    return qHash (qs);
  }
};

// set with hashed unique index that allow for efficient lookup of
// entity by internal id
typedef multi_index_container<
  entity,
  indexed_by<
    hashed_unique<tag<id>, member<entity, int, &entity::id_> >,
    hashed_unique<tag<primary_prefix>, member<entity, QString, &entity::primary_prefix_>, hash_QString> >
  > entities_type;

struct prefix
{
  explicit prefix (QString const& prefix, bool exact_match_only, int entity_id)
    : prefix_ {prefix}
    , exact_ {exact_match_only}
    , entity_id_ {entity_id}
  {
  }

  // extract key which is the prefix ignoring the trailing override
  // information
  QString prefix_key () const
  {
    auto const& prefix = prefix_.toStdString ();
    return QString::fromStdString (prefix.substr (0, prefix.find_first_of ("({[<~")));
  }
    
  QString prefix_;              // call or prefix with optional
                                // trailing override information
  bool exact_;
  int entity_id_;
};

#if !defined (QT_NO_DEBUG_STREAM)
QDebug operator << (QDebug dbg, prefix const& p)
{
  QDebugStateSaver saver {dbg};
  dbg.nospace () << "prefix("
                 << p.prefix_ << ", "
                 << p.exact_ << ", "
                 << p.entity_id_ << ')';
  return dbg;
}
#endif

// set with ordered unique index that allow for efficient
// determination of entity and entity overrides for a call or call
// prefix
typedef multi_index_container<
  prefix,
  indexed_by<
    ordered_unique<const_mem_fun<prefix, QString, &prefix::prefix_key> > >
  > prefixes_type;

class AD1CCty::impl final
{
public:
  using entity_by_id = entities_type::index<id>::type;

  explicit impl (Configuration const * configuration)
    : configuration_ {configuration}
  {
  }

  QString get_cty_path(const Configuration *configuration);
  void load_cty(QFile &file);

  entity_by_id::iterator lookup_entity (QString call, prefix const& p) const
  {
    call = call.toUpper ();
    entity_by_id::iterator e;   // iterator into entity set

    //
    // deal with special rules that cty.dat does not cope with
    //
    if (call.startsWith ("KG4") && call.size () != 5 && call.size () != 3)
      {
        // KG4 2x1 and 2x3 calls that map to Gitmo are mainland US not Gitmo
        return entities_.project<id> (entities_.get<primary_prefix> ().find ("K"));
      }
    else
      {
        return entities_.get<id> ().find (p.entity_id_);
      }
  }

  Record fixup (prefix const& p, entity const& e) const
  {
    Record result;
    result.continent = e.continent_;
    result.CQ_zone = e.CQ_zone_;
    result.ITU_zone = e.ITU_zone_;
    result.entity_name = e.name_;
    result.WAE_only = e.WAE_only_;
    result.latitude = e.lat_;
    result.longtitude = e.long_;
    result.UTC_offset = e.UTC_offset_;
    result.primary_prefix = e.primary_prefix_;

    // check for overrides
    bool ok1 {true}, ok2 {true}, ok3 {true}, ok4 {true}, ok5 {true};
    QString value;
    if (override_value (p.prefix_, '(', ')', value)) result.CQ_zone = value.toInt (&ok1);
    if (override_value (p.prefix_, '[', ']', value)) result.ITU_zone = value.toInt (&ok2);
    if (override_value (p.prefix_, '<', '>', value))
      {
        auto const& fix = value.split ('/');
        result.latitude = fix[0].toFloat (&ok3);
        result.longtitude = fix[1].toFloat (&ok4);
      }
    if (override_value (p.prefix_, '{', '}', value)) result.continent = continent (value);
    if (override_value (p.prefix_, '~', '~', value)) result.UTC_offset = static_cast<int> (value.toFloat (&ok5) * 60 * 60);
    if (!(ok1 && ok2 && ok3 && ok4 && ok5))
      {
        throw std::domain_error {"Invalid number in cty.dat for override of " + p.prefix_.toStdString ()};
      }
    return result;
  }

  static bool override_value (QString const& s, QChar lb, QChar ub, QString& v)
  {
    auto pos = s.indexOf (lb);
    if (pos >= 0)
      {
        v = s.mid (pos + 1, s.indexOf (ub, pos + 1) - pos - 1);
        return true;
      }
    return false;
  }

  Configuration const * configuration_;
  QString path_;
  QString cty_version_;
  QString cty_version_date_;

  entities_type entities_;
  prefixes_type prefixes_;
};

AD1CCty::Record::Record ()
  : continent {Continent::UN}
  , CQ_zone {0}
  , ITU_zone {0}
  , WAE_only {false}
  , latitude {NAN}
  , longtitude {NAN}
  , UTC_offset {0}
{
}

#if !defined (QT_NO_DEBUG_STREAM)
  QDebug operator << (QDebug dbg, AD1CCty::Record const& r)
  {
    QDebugStateSaver saver {dbg};
    dbg.nospace () << "AD1CCty::Record("
                   << r.continent << ", "
                   << r.CQ_zone << ", "
                   << r.ITU_zone << ", "
                   << r.entity_name << ", "
                   << r.WAE_only << ", "
                   << r.latitude << ", "
                   << r.longtitude << ", "
                   << (r.UTC_offset / (60. * 60.)) << ", "
                   << r.primary_prefix << ')';
    return dbg;
  }
#endif

auto AD1CCty::continent (QString const& continent_id) -> Continent
{
  Continent continent;
  if ("AF" == continent_id)
    {
      continent = Continent::AF;
    }
  else if ("AN" == continent_id)
    {
      continent = Continent::AN;
    }
  else if ("AS" == continent_id)
    {
      continent = Continent::AS;
    }
  else if ("EU" == continent_id)
    {
      continent = Continent::EU;
    }
  else if ("NA" == continent_id)
    {
      continent = Continent::NA;
    }
  else if ("OC" == continent_id)
    {
      continent = Continent::OC;
    }
  else if ("SA" == continent_id)
    {
      continent = Continent::SA;
    }
  else
    {
      throw std::domain_error {"Invalid continent id: " + continent_id.toStdString ()};
    }
  return continent;
}

char const * AD1CCty::continent (Continent c)
{
  switch (c)
    {
    case Continent::AF: return "AF";
    case Continent::AN: return "AN";
    case Continent::AS: return "AS";
    case Continent::EU: return "EU";
    case Continent::NA: return "NA";
    case Continent::OC: return "OC";
    case Continent::SA: return "SA";
    default: return "UN";
    }
}

QString AD1CCty::impl::get_cty_path(Configuration const * configuration)
{
  QDir dataPath {QStandardPaths::writableLocation (QStandardPaths::DataLocation)};
  auto path = dataPath.exists (file_name)
              ? dataPath.absoluteFilePath (file_name) // user override
              : configuration->data_dir ().absoluteFilePath (file_name); // or original
  return path;
}

void AD1CCty::impl::load_cty(QFile &file)
{
  QRegularExpression version_pattern{R"(VER\d{8})"};
  int entity_id = 0;
  int line_number{0};

  entities_.clear();
  prefixes_.clear();
  cty_version_ = QString{};
  cty_version_date_ = QString{};

  QTextStream in{&file};
  while (!in.atEnd())
  {
    auto const &entity_line = in.readLine();
    ++line_number;
    if (!in.atEnd())
    {
      auto const &entity_parts = entity_line.split(':');
      if (entity_parts.size() >= 8)
      {
        auto primary_prefix = entity_parts[7].trimmed();
        bool WAE_only{false};
        if (primary_prefix.startsWith('*'))
        {
          primary_prefix = primary_prefix.mid(1);
          WAE_only = true;
        }
        bool ok1, ok2, ok3, ok4, ok5;
        entities_.emplace(++entity_id, entity_parts[0].trimmed(), WAE_only, entity_parts[1].trimmed().toInt(&ok1),
                          entity_parts[2].trimmed().toInt(&ok2), continent(entity_parts[3].trimmed()),
                          entity_parts[4].trimmed().toFloat(&ok3), entity_parts[5].trimmed().toFloat(&ok4),
                          static_cast<int> (entity_parts[6].trimmed().toFloat(&ok5) * 60 * 60), primary_prefix);
        if (!(ok1 && ok2 && ok3 && ok4 && ok5))
        {
          throw std::domain_error{"Invalid number in cty.dat line " + boost::lexical_cast<std::string>(line_number)};
        }
        QString line;
        QString detail;
        do
        {
          in.readLineInto(&line);
          ++line_number;
        } while (detail += line, !detail.endsWith(';'));
        for (auto prefix: detail.left(detail.size() - 1).split(','))
        {
          prefix = prefix.trimmed();
          bool exact{false};
          if (prefix.startsWith('='))
          {
            prefix = prefix.mid(1);
            exact = true;
            // match version pattern to prefix
            if (version_pattern.match(prefix).hasMatch())
            {
              cty_version_date_ = prefix;
            }
          }
          prefixes_.emplace(prefix, exact, entity_id);
        }
      }
    }
  }
}

AD1CCty::AD1CCty (Configuration const * configuration)
  : m_ {configuration}
{
  Q_ASSERT (configuration);
  // TODO: G4WJS - consider doing the following asynchronously to
  // speed up startup. Not urgent as it takes less than 0.5s on a Core
  // i7 reading BIG CTY.DAT.
  AD1CCty::reload (configuration);

  //NJ0A
  gridNumPrefix = 0;

  for (int i = 0; i < maxPrefix ; i ++) {
      for (int j = 0; j < maxIndex; j ++) {
          gridState[i] [j] = "**";
      }
  }

  QDir dataPath {QStandardPaths::writableLocation (QStandardPaths::DataLocation)};
  m_->path_ = dataPath.exists (file_name)
    ? dataPath.absoluteFilePath (file_name) // user override
    : configuration->data_dir ().absoluteFilePath (file_name); // or original

  QString path = dataPath.exists (grid_file_name)
   ? dataPath.absoluteFilePath (grid_file_name) // user override
   : configuration->data_dir ().absoluteFilePath (grid_file_name);   // or original in the resources FS


  QFile file1 {path};

  if (file1.open (QFile::ReadOnly))
    {
       int line_number [[maybe_unused]] {0};
       QTextStream in {&file1};
       while (!in.atEnd ())
       {
          auto const& entity_line = in.readLine ();
          ++line_number;
          if (!in.atEnd () && entity_line.length() > 0 && entity_line.contains("<"))
          {
              //std::cout << entity_line.toStdString() << '\n';
              if (entity_line.contains("<")) {
                auto const& entity_parts = entity_line.split ('<');
                //std::cout << "Grid prefix: " << entity_parts[0].toStdString() << '\n';
                gridPrefix[gridNumPrefix] = entity_parts[0];
                gridNumPrefix++;
                while (!in.atEnd()) {
                    auto const& entity_grid_line = in.readLine();
                    if (entity_grid_line.length()  > 1 &&
                            entity_grid_line.contains(":") &&
                            entity_grid_line.contains(",")) {
                        auto const& entity_parts = entity_grid_line.split(":");
                        //std::cout << "Grid Indxes: " << entity_parts[0].trimmed().toStdString() << " " ;

                        auto const& entity_temp = entity_parts[1].split(",");
                        auto const & entity_state = entity_temp[0];
                        //std::cout << "State: " << entity_state.toStdString() << '\n';
                        gridState[gridNumPrefix - 1] [entity_parts[0].toInt()] = entity_state;

                    } else if (entity_grid_line.length()  > 1 &&
                               entity_grid_line.contains(":") &&
                               entity_grid_line.contains(">")) {
                        auto const& entity_parts = entity_grid_line.split(":");
                        //std::cout << "Grid Indxes: " <<  entity_parts[0].trimmed().toStdString() << " ";

                        auto const& entity_temp = entity_parts[1].split(">");
                        auto const & entity_state = entity_temp[0];
                        //std::cout << "State: " << entity_state.toStdString() << '\n';
                        gridState[gridNumPrefix - 1] [entity_parts[0].toInt()] = entity_state;

                        break;
                    }
                }
              }
          }
       }
    }
  }

void AD1CCty::reload(Configuration const * configuration)
{
  m_->path_ = m_->impl::get_cty_path(configuration);
  QFile file {m_->path_};

  LOG_INFO(QString{"Loading CTY.DAT from %1"}.arg (m_->path_));

  if (file.open (QFile::ReadOnly))
  {
    m_->impl::load_cty(file);
    m_->cty_version_ = AD1CCty::lookup("VERSION").entity_name;
    Q_EMIT cty_loaded(m_->cty_version_);
    LOG_INFO(QString{"Loaded CTY.DAT version %1, %2"}.arg (m_->cty_version_date_).arg (m_->cty_version_));
  }
}

AD1CCty::~AD1CCty ()
{
}

auto AD1CCty::lookup (QString const& call) const -> Record
{
  auto const& exact_search = call.toUpper ();

  // CB 27MHz callsigns NNNLLNNN (e.g. 001AB123): map the first 3 digits to CB country
  auto cb_country = cb_country_name_from_call (exact_search);
  if (!cb_country.isEmpty ())
    {
      Record r;
      r.entity_name = cb_country;
      r.primary_prefix = Radio::base_callsign (exact_search).left (3).toUpper ();
      return r;
    }

  if (!(exact_search.endsWith ("/MM") || exact_search.endsWith ("/AM")))
    {
      auto search_prefix = Radio::effective_prefix (exact_search);
      if (search_prefix != exact_search)
        {
          auto p = m_->prefixes_.find (exact_search);
          if (p != m_->prefixes_.end () && p->exact_)
            {
              return m_->fixup (*p, *m_->lookup_entity (call, *p));
            }
        }
      while (search_prefix.size ())
        {
          auto p = m_->prefixes_.find (search_prefix);
          if (p != m_->prefixes_.end ())
            {
              impl::entity_by_id::iterator e = m_->lookup_entity (call, *p);
              // always lookup WAE entities, we substitute them later in displaytext.cpp if "Include extra WAE entites" is not selected
              if (!p->exact_ || call.size () == search_prefix.size ())
                {
                  return m_->fixup (*p, *e);
                }
            }
          search_prefix = search_prefix.left (search_prefix.size () - 1);
        }
    }
  return Record {};
}

auto AD1CCty::version () const -> QString
{
  return m_->cty_version_date_;
}

// NJ0A
auto AD1CCty::findState ( QString const& grid) const -> QString
{


    auto const& prefix = grid.left(2);
    int gridIndex = grid.mid(2,2).toInt();
    //if (gridIndex < 0 || gridIndex > maxIndex) {
        //std::cout << "Prefix: " << prefix.toStdString() << " Index " << gridIndex << '\n';
    //}
    for (int i = 0; i < gridNumPrefix + 1; i ++) {
        if (gridPrefix[i] == prefix) {
            QString state = gridState[i] [gridIndex];
            //if (state.length() < 2 ) {
               // std::cout << "Prefix: " << prefix.toStdString() << " Index " << gridIndex << '\n';
            //}
            return state;
        }
    }

    return "**";
} // NJ0A
