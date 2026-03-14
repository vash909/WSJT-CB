#include "FrequencyList.hpp"

#include <cstdlib>
#include <utility>
#include <limits>
#include <algorithm>


#include <QMetaType>
#include <QAbstractTableModel>
#include <QString>
#include <QList>
#include <QListIterator>
#include <QVector>
#include <QStringList>
#include <QMimeData>
#include <QTextStream>
#include <QDataStream>
#include <QByteArray>
#include <QDebugStateSaver>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>
#include <QFile>
#include <QException>

#include "Radio.hpp"
#include "Bands.hpp"
#include "pimpl_impl.hpp"
#include "revision_utils.hpp"
#include "Logger.hpp"

#include "moc_FrequencyList.cpp"

namespace
{
  FrequencyList_v2_101::FrequencyItems const default_frequency_list =
    {
      {27265000, Modes::FT8, IARURegions::ALL, "CB", "", QDateTime(), QDateTime(), true},
      {27045000, Modes::FT8, IARURegions::ALL, "CB (2nd)", "", QDateTime(), QDateTime(), false},
    };
}

#if !defined (QT_NO_DEBUG_STREAM)
QDebug operator << (QDebug debug, FrequencyList_v2_101::Item const& item)
{
  QDebugStateSaver saver {debug};
  return debug.nospace () << item.toString ();
}
#endif
bool FrequencyList_v2_101::Item::isSane() const
{
  return frequency_ > 0.0 && (!start_time_.isValid() || !end_time_.isValid() || start_time_ < end_time_)
  && (region_ == IARURegions::ALL || region_ == IARURegions::R1 || region_ == IARURegions::R2 || region_ == IARURegions::R3);
}

QString FrequencyList_v2_101::Item::toString () const
{
  QString string;
  QTextStream qts {&string};
  qts << "FrequencyItem("
      << Radio::frequency_MHz_string (frequency_) << ", "
      << IARURegions::name (region_) << ", "
      << Modes::name (mode_) << ", "
      << start_time_.toString(Qt::ISODate) << ", "
      << end_time_.toString(Qt::ISODate) << ", "
      << description_ << ", "
      << source_ << ","
      << preferred_ << ')';
  return string;
}

QJsonObject FrequencyList_v2_101::Item::toJson() const {
  return {{"frequency", Radio::frequency_MHz_string (frequency_) },
          {"mode", Modes::name (mode_) },
          {"region", IARURegions::name (region_)},
          {"description", description_},
          {"source", source_},
          {"start_time", start_time_.toString(Qt::ISODate) },
          {"end_time", end_time_.toString(Qt::ISODate) },
          {"preferred", preferred_}};
}

QDataStream& operator << (QDataStream& os, FrequencyList_v2_101::Item const& item)
{
  return os << item.frequency_
            << item.mode_
            << item.region_
            << item.start_time_
            << item.end_time_
            << item.description_
            << item.source_
            << item.preferred_;
}

QDataStream& operator >> (QDataStream& is, FrequencyList_v2_101::Item& item)
{
  return is >> item.frequency_
            >> item.mode_
            >> item.region_
            >> item.start_time_
            >> item.end_time_
            >> item.description_
            >> item.source_
            >> item.preferred_;
}

class FrequencyList_v2_101::impl final
  : public QAbstractTableModel
{
public:
  impl (Bands const * bands, QObject * parent)
    : QAbstractTableModel {parent}
    , bands_ {bands}
    , region_filter_ {IARURegions::ALL}
    , mode_filter_ {Modes::ALL}
    , filter_on_time_ {false}
  {
  }

  FrequencyItems frequency_list (FrequencyItems);
  QModelIndex add (Item);
  void add (FrequencyItems);

  // Implement the QAbstractTableModel interface
  int rowCount (QModelIndex const& parent = QModelIndex {}) const override;
  int columnCount (QModelIndex const& parent = QModelIndex {}) const override;
  Qt::ItemFlags flags (QModelIndex const& = QModelIndex {}) const override;
  QVariant data (QModelIndex const&, int role = Qt::DisplayRole) const override;
  bool setData (QModelIndex const&, QVariant const& value, int role = Qt::EditRole) override;
  QVariant headerData (int section, Qt::Orientation, int = Qt::DisplayRole) const override;
  bool removeRows (int row, int count, QModelIndex const& parent = QModelIndex {}) override;
  bool insertRows (int row, int count, QModelIndex const& parent = QModelIndex {}) override;
  QStringList mimeTypes () const override;
  QMimeData * mimeData (QModelIndexList const&) const override;

  void unprefer_all_but(Item & item, int const row, QVector<int> );

  static int constexpr num_cols {SENTINAL};
  static auto constexpr mime_type = "application/wsjt.Frequencies";

  Bands const * bands_;
  FrequencyItems frequency_list_;
  Region region_filter_;
  Mode mode_filter_;
  bool filter_on_time_;

};

FrequencyList_v2_101::FrequencyList_v2_101 (Bands const * bands, QObject * parent)
  : QSortFilterProxyModel {parent}
  , m_ {bands, parent}
{
  setSourceModel (&*m_);
  setSortRole (SortRole);
}

FrequencyList_v2_101::~FrequencyList_v2_101 ()
{
}

auto FrequencyList_v2_101::frequency_list (FrequencyItems frequency_list) -> FrequencyItems
{
  return m_->frequency_list (frequency_list);
}

auto FrequencyList_v2_101::frequency_list () const -> FrequencyItems const&
{
  return m_->frequency_list_;
}

auto FrequencyList_v2_101::frequency_list (QModelIndexList const& model_index_list) const -> FrequencyItems
{
  FrequencyItems list;
  Q_FOREACH (auto const& index, model_index_list)
    {
      list << m_->frequency_list_[mapToSource (index).row ()];
    }
  return list;
}

void FrequencyList_v2_101::frequency_list_merge (FrequencyItems const& items)
{
  m_->add (items);
}


int FrequencyList_v2_101::best_working_frequency (Frequency f) const
{
  int result {-1};
  auto const& target_band = m_->bands_->find (f);
  if (!target_band.isEmpty ())
    {
      Radio::FrequencyDelta delta {std::numeric_limits<Radio::FrequencyDelta>::max ()};
      // find a frequency in the same band that is allowed
      for (int row = 0; row < rowCount (); ++row)
        {
          auto const& source_row = mapToSource (index (row, 0)).row ();
          auto const& candidate_frequency = m_->frequency_list_[source_row].frequency_;
          auto const& band = m_->bands_->find (candidate_frequency);
          if (band == target_band)
            {
              // take the preferred one
              if (m_->frequency_list_[source_row].preferred_)
                {
                  return row;
                }
              // take closest band match
              Radio::FrequencyDelta new_delta = f - candidate_frequency;
              if (std::abs (new_delta) < std::abs (delta))
                {
                  delta = new_delta;
                  result = row;
                }
            }
        }
    }
  return result;
}

int FrequencyList_v2_101::best_working_frequency (QString const& target_band) const
{
  int result {-1};
  if (!target_band.isEmpty ())
    {
      // find a frequency in the same band that is allowed
      for (int row = 0; row < rowCount (); ++row)
        {
          auto const& source_row = mapToSource (index (row, 0)).row ();
          auto const& band = m_->bands_->find (m_->frequency_list_[source_row].frequency_);
          if (band == target_band)
            {
               if (m_->frequency_list_[source_row].preferred_)
                 return row; // return the preferred one immediately
               result = row;
            }
        }
    }
  return result;
}

void FrequencyList_v2_101::reset_to_defaults ()
{
  m_->frequency_list (default_frequency_list);
}

QModelIndex FrequencyList_v2_101::add (Item f)
{
  return mapFromSource (m_->add (f));
}

bool FrequencyList_v2_101::remove (Item f)
{
  auto row = m_->frequency_list_.indexOf (f);

  if (0 > row)
    {
      return false;
    }

  return m_->removeRow (row);
}

bool FrequencyList_v2_101::removeDisjointRows (QModelIndexList rows)
{
  bool result {true};

  // We must work with source model indexes because we don't want row
  // removes to invalidate model indexes we haven't yet processed. We
  // achieve that by processing them in descending row order.
  for (int r = 0; r < rows.size (); ++r)
    {
      rows[r] = mapToSource (rows[r]);
    }

  // reverse sort by row
  std::sort (rows.begin (), rows.end (), [] (QModelIndex const& lhs, QModelIndex const& rhs)
             {
               return rhs.row () < lhs.row (); // reverse row ordering
             });
  Q_FOREACH (auto index, rows)
    {
      if (result && !m_->removeRow (index.row ()))
        {
          result = false;
        }
    }
  return result;
}

void FrequencyList_v2_101::filter (Region region, Mode mode, bool filter_on_time)
{
  m_->region_filter_ = region;
  m_->mode_filter_ = mode;
  m_->filter_on_time_ = filter_on_time;
  invalidateFilter ();
}

void FrequencyList_v2_101::filter_refresh ()
{
  invalidateFilter ();
}

bool FrequencyList_v2_101::filterAcceptsRow (int source_row, QModelIndex const& /* parent */) const
{
  bool result {true};
  auto const& item = m_->frequency_list_[source_row];
  if (m_->region_filter_ != IARURegions::ALL)
    {
      result = IARURegions::ALL == item.region_ || m_->region_filter_ == item.region_;
    }
  if (result && m_->mode_filter_ != Modes::ALL)
    {
      // we pass ALL mode rows unless filtering for FreqCal mode
      result = (Modes::ALL == item.mode_ && m_->mode_filter_ != Modes::FreqCal)
        || m_->mode_filter_ == item.mode_;
    }
  if (result && m_->filter_on_time_)
    {
      result = (!item.start_time_.isValid() || item.start_time_ <= QDateTime::currentDateTimeUtc ()) &&
              (!item.end_time_.isValid() || item.end_time_ >= QDateTime::currentDateTimeUtc ());
    }
  return result;
}


auto FrequencyList_v2_101::impl::frequency_list (FrequencyItems frequency_list) -> FrequencyItems
{
  beginResetModel ();
  std::swap (frequency_list_, frequency_list);
  endResetModel ();
  return frequency_list;
}

// add a frequency returning the new model index
QModelIndex FrequencyList_v2_101::impl::add (Item f)
{
  // Any Frequency that isn't in the list may be added
  if (!frequency_list_.contains (f))
    {
      auto row = frequency_list_.size ();

      beginInsertRows (QModelIndex {}, row, row);
      frequency_list_.append (f);
      endInsertRows ();

      // if we added one that had a preferred frequency, unprefer everything else
      if (f.preferred_) unprefer_all_but(f, row, {Qt::DisplayRole, Qt::CheckStateRole});

      return index (row, 0);
    }
  return QModelIndex {};
}

void FrequencyList_v2_101::impl::add (FrequencyItems items)
{
  // Any Frequency that isn't in the list may be added
  for (auto p = items.begin (); p != items.end ();)
    {
      if (frequency_list_.contains (*p))
        {
          p = items.erase (p);
        }
      else
        {
          ++p;
        }
    }

  if (items.size ())
    {
      auto row = frequency_list_.size ();

      beginInsertRows (QModelIndex {}, row, row + items.size () - 1);
      frequency_list_.append (items);
      endInsertRows ();
    }
}

int FrequencyList_v2_101::impl::rowCount (QModelIndex const& parent) const
{
  return parent.isValid () ? 0 : frequency_list_.size ();
}

int FrequencyList_v2_101::impl::columnCount (QModelIndex const& parent) const
{
  return parent.isValid () ? 0 : num_cols;
}

Qt::ItemFlags FrequencyList_v2_101::impl::flags (QModelIndex const& index) const
{
  auto result = QAbstractTableModel::flags (index) | Qt::ItemIsDropEnabled;
  auto row = index.row ();
  auto column = index.column ();
  if (index.isValid ()
      && row < frequency_list_.size ()
      && column < num_cols)
    {
      if (frequency_mhz_column != column)
        {
          result |= Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
        }

      if (preferred_column == column)
        {
          result |= Qt::ItemIsUserCheckable;
        }
    }
  return result;
}

QVariant FrequencyList_v2_101::impl::data (QModelIndex const& index, int role) const
{
  QVariant item;

  auto const& row = index.row ();
  auto const& column = index.column ();

  if (index.isValid ()
      && row < frequency_list_.size ()
      && column < num_cols)
    {
      auto const& frequency_item = frequency_list_.at (row);
      switch (column)
        {
        case region_column:
          switch (role)
            {
            case SortRole:
            case Qt::DisplayRole:
            case Qt::EditRole:
            case Qt::AccessibleTextRole:
              item = IARURegions::name (frequency_item.region_);
              break;

            case Qt::ToolTipRole:
            case Qt::AccessibleDescriptionRole:
              item = tr ("IARU Region");
              break;

            case Qt::TextAlignmentRole:
              item = Qt::AlignHCenter + Qt::AlignVCenter;
              break;
            }
          break;

        case mode_column:
          switch (role)
            {
            case SortRole:
            case Qt::DisplayRole:
            case Qt::EditRole:
            case Qt::AccessibleTextRole:
              item = Modes::name (frequency_item.mode_);
              break;

            case Qt::ToolTipRole:
            case Qt::AccessibleDescriptionRole:
              item = tr ("Mode");
              break;

            case Qt::TextAlignmentRole:
              item = Qt::AlignHCenter + Qt::AlignVCenter;
              break;
            }
          break;

        case frequency_column:
          switch (role)
            {
            case SortRole:
            case Qt::EditRole:
            case Qt::AccessibleTextRole:
              item = frequency_item.frequency_;
              break;

            case Qt::DisplayRole:
              {
                auto const& band = bands_->find (frequency_item.frequency_);
                item = Radio::pretty_frequency_MHz_string (frequency_item.frequency_)
                  + " MHz (" + (band.isEmpty () ? "OOB" : band) + ')';
              }
              break;

            case Qt::ToolTipRole:
            case Qt::AccessibleDescriptionRole:
              item = tr ("Frequency");
              break;

            case Qt::TextAlignmentRole:
              item = Qt::AlignRight + Qt::AlignVCenter;
              break;
            }
          break;

        case frequency_mhz_column:
          switch (role)
            {
            case Qt::EditRole:
            case Qt::AccessibleTextRole:
              item = Radio::frequency_MHz_string (frequency_item.frequency_);
              break;

            case Qt::DisplayRole:
              {
                auto const& band = bands_->find (frequency_item.frequency_);
                QString desc_text;
                desc_text = frequency_item.description_.isEmpty() ? "" : " \u2502 " + frequency_item.description_;
                item = (frequency_item.preferred_ ? "\u2055 " : "") +
                       Radio::pretty_frequency_MHz_string(frequency_item.frequency_)
                       + " MHz (" + (band.isEmpty() ? "OOB" : band) + ")" +
                        (((frequency_item.start_time_.isValid() && !frequency_item.start_time_.isNull()) ||
                        (frequency_item.end_time_.isValid() && !frequency_item.end_time_.isNull())) ? " \u2502 " : "")
                       + desc_text;
              }
              break;

            case Qt::ToolTipRole:
            case Qt::AccessibleDescriptionRole:
              item = tr ("Frequency (MHz)");
              break;

            case Qt::TextAlignmentRole:
              item = Qt::AlignRight + Qt::AlignVCenter;
              break;
            }
            break;

          case description_column:
            switch (role)
              {
                case SortRole:
                case Qt::DisplayRole:
                case Qt::EditRole:
                case Qt::AccessibleTextRole:
                  item = frequency_item.description_;
                break;

                case Qt::ToolTipRole:
                case Qt::AccessibleDescriptionRole:
                  item = tr("Description");
                break;

                case Qt::TextAlignmentRole:
                  item = Qt::AlignLeft + Qt::AlignVCenter;
                break;
              }
            break;

          case source_column:
            switch (role)
              {
                case SortRole:
                case Qt::DisplayRole:
                case Qt::EditRole:
                case Qt::AccessibleTextRole:
                  item = frequency_item.source_;
                break;

                case Qt::ToolTipRole:
                case Qt::AccessibleDescriptionRole:
                  item = tr ("Source");
                break;

                case Qt::TextAlignmentRole:
                  item = Qt::AlignLeft + Qt::AlignVCenter;
                break;
              }
          break;

          case start_time_column:
            switch (role)
              {
                case SortRole:
                  item = frequency_item.start_time_;
                break;

                case Qt::EditRole:
                  if (frequency_item.start_time_.isNull () || !frequency_item.start_time_.isValid ())
                    {
                      item = QDateTime::currentDateTimeUtc ().toString (Qt::ISODate);
                    }
                    else
                    {
                      item = frequency_item.start_time_.toString(Qt::ISODate);
                    }
                break;

                case Qt::DisplayRole:
                case Qt::AccessibleTextRole:
                  item = frequency_item.start_time_.toString(Qt::ISODate);
                break;

                case Qt::ToolTipRole:
                case Qt::AccessibleDescriptionRole:
                  item = tr ("Start Time");
                break;

                case Qt::TextAlignmentRole:
                  item = Qt::AlignLeft + Qt::AlignVCenter;
                break;
              }
          break;

          case end_time_column:
            switch (role)
              {
                case SortRole:
                  item = frequency_item.end_time_;
                break;

                case Qt::EditRole:
                  if (frequency_item.end_time_.isNull () || !frequency_item.end_time_.isValid ())
                    {
                      item = QDateTime::currentDateTimeUtc ().toString (Qt::ISODate);
                    }
                    else
                    {
                      item = frequency_item.end_time_.toString(Qt::ISODate);
                    }
                break;

                case Qt::DisplayRole:
                case Qt::AccessibleTextRole:
                  item = frequency_item.end_time_.toString(Qt::ISODate);
                break;

                case Qt::ToolTipRole:
                case Qt::AccessibleDescriptionRole:
                  item = tr ("End Time");
                break;

                case Qt::TextAlignmentRole:
                  item = Qt::AlignLeft + Qt::AlignVCenter;
                break;
              }
          break;

          case preferred_column:
            switch (role)
              {
                case SortRole:
                  item = frequency_item.preferred_;
                break;
                case Qt::DisplayRole:
                case Qt::EditRole:
                case Qt::AccessibleTextRole:
                  //item = frequency_item.preferred_ ? QString("True") : QString("False");
                break;

                case Qt::ToolTipRole:
                case Qt::AccessibleDescriptionRole:
                  item = tr ("Pref");
                break;

                case Qt::TextAlignmentRole:
                  item = Qt::AlignHCenter + Qt::AlignVCenter;
                break;

                case Qt::CheckStateRole:
                  item = frequency_item.preferred_ ? Qt::Checked : Qt::Unchecked;
                break;
              }
          break;
        }
    }
  return item;
}

void FrequencyList_v2_101::impl::unprefer_all_but(Item &item, int const item_row, QVector<int> roles)
{
  // un-prefer all of the other frequencies in this band
  auto const band = bands_->find (item.frequency_);
  if (band.isEmpty ()) return; // out of any band

  roles << Qt::CheckStateRole;
  roles << Qt::DisplayRole;

  for (int row = 0; row < rowCount (); ++row)
  {
      if (row == item_row) continue;

      Item &i = frequency_list_[row];
      auto const &iter_band = bands_->find(i.frequency_);

      if (!iter_band.isEmpty() && band == iter_band && (i.region_ == item.region_) && (i.mode_ == item.mode_))
      {
          i.preferred_ = false;
          Q_EMIT dataChanged(index(row,preferred_column), index(row,preferred_column), roles);
      }
  }
}

bool FrequencyList_v2_101::impl::setData (QModelIndex const& model_index, QVariant const& value, int role)
{
  bool changed {false};
  auto const& row = model_index.row ();
  auto& item = frequency_list_[row];

  QVector<int> roles;
  roles << role;

  if (model_index.isValid ()
      && Qt::CheckStateRole == role
      && row < frequency_list_.size ()
      && model_index.column () == preferred_column)
    {
      bool b_val = ((Qt::CheckState)value.toInt() == Qt::Checked);
      if (b_val != item.preferred_)
        {
          item.preferred_ = b_val;
          if (item.preferred_)
            {
              unprefer_all_but (item, row, roles); // un-prefer all of the other frequencies in this band
            }
          Q_EMIT dataChanged(index(row,description_column), index(row,preferred_column), roles);
          changed = true;
        }
    }

  if (model_index.isValid ()
      && Qt::EditRole == role
      && row < frequency_list_.size ())
    {
      switch (model_index.column())
        {
          case region_column:
            {
              auto region = IARURegions::value(value.toString());
              if (region != item.region_)
                {
                  item.region_ = region;
                  Q_EMIT dataChanged(model_index, model_index, roles);
                  changed = true;
                }
            }
          break;

          case mode_column:
            {
              auto mode = Modes::value(value.toString());
              if (mode != item.mode_)
                {
                  item.mode_ = mode;
                  Q_EMIT dataChanged(model_index, model_index, roles);
                  changed = true;
                }
            }
          break;

          case frequency_column:
            {
              if (value.canConvert<Frequency>())
                {
                  Radio::Frequency frequency{qvariant_cast<Radio::Frequency>(value)};
                  if (frequency != item.frequency_)
                    {
                      item.frequency_ = frequency;
                      // mark derived column (1) changed as well
                      Q_EMIT dataChanged(index(model_index.row(), 1), model_index, roles);
                      changed = true;
                    }
                }
            }
          break;

          case description_column:
            {
              if (value.toString() != item.description_)
                {
                  item.description_ = value.toString();
                  Q_EMIT dataChanged(model_index, model_index, roles);
                  changed = true;
                }
            }
          break;

          case source_column:
            {
              if (value.toString() != item.source_)
                {
                  item.source_ = value.toString();
                  Q_EMIT dataChanged(model_index, model_index, roles);
                  changed = true;
                }
            }
          break;

          case start_time_column:
            {
              QDateTime start_time = QDateTime::fromString(value.toString(), Qt::ISODate);
              LOG_INFO(QString{"start_time = %1 - isEmpty %2"}.arg(value.toString()).arg(value.toString().isEmpty()));
              if (value.toString().isEmpty())
                { // empty string is valid
                  start_time = QDateTime();
                }
              if (start_time.isValid() || start_time.isNull())
                {
                  item.start_time_ = start_time;
                  if (item.end_time_.isValid() && !item.start_time_.isNull() && item.end_time_ < item.start_time_)
                    {
                      item.end_time_ = item.start_time_;
                    }
                  Q_EMIT dataChanged(model_index, index(model_index.row(), end_time_column), roles);
                  changed = true;
                }
            }
          break;

          case end_time_column:
            {
              QDateTime end_time = QDateTime::fromString(value.toString(), Qt::ISODate);
              if (value.toString().isEmpty())
                { // empty string is valid
                  end_time = QDateTime();
                }
              if (end_time.isValid() || end_time.isNull())
                {
                  item.end_time_ = end_time;
                  if (item.start_time_.isValid() && !item.end_time_.isNull() && end_time <= item.start_time_)
                    {
                      item.start_time_ = end_time;
                    }
                  Q_EMIT dataChanged(index(model_index.row(), start_time_column), model_index, roles);
                  changed = true;
                }
            }
          break;

          case preferred_column:
            {
              bool b_value = value.toBool();
              if (b_value != item.preferred_)
                {
                  item.preferred_ = b_value;
                  Q_EMIT dataChanged(index(model_index.row(), start_time_column), model_index, roles);
                  changed = true;
                }
            }
          break;

        }
    }

  return changed;
}

QVariant FrequencyList_v2_101::impl::headerData (int section, Qt::Orientation orientation, int role) const
{
  QVariant header;
  if (Qt::DisplayRole == role
      && Qt::Horizontal == orientation
      && section < num_cols)
    {
      switch (section)
        {
          case region_column: header = tr ("IARU Region"); break;
          case mode_column: header = tr ("Mode"); break;
          case frequency_column: header = tr ("Frequency"); break;
          case frequency_mhz_column: header = tr ("Frequency (MHz)"); break;
          case source_column: header = tr ("Source"); break;
          case start_time_column: header = tr ("Start Date/Time"); break;
          case end_time_column: header = tr ("End Date/Time"); break;
          case preferred_column: header = tr ("Pref"); break;
          case description_column: header = tr ("Description"); break;
        }
    }
  else
    {
      header = QAbstractTableModel::headerData (section, orientation, role);
    }
  return header;
}

bool FrequencyList_v2_101::impl::removeRows (int row, int count, QModelIndex const& parent)
{
  if (0 < count && (row + count) <= rowCount (parent))
    {
      beginRemoveRows (parent, row, row + count - 1);
      for (auto r = 0; r < count; ++r)
        {
          frequency_list_.removeAt (row);
        }
      endRemoveRows ();
      return true;
    }
  return false;
}

bool FrequencyList_v2_101::impl::insertRows (int row, int count, QModelIndex const& parent)
{
  if (0 < count)
    {
      beginInsertRows (parent, row, row + count - 1);
      for (auto r = 0; r < count; ++r)
        {
          frequency_list_.insert (row, Item {0, Mode::ALL, IARURegions::ALL, QString(), QString(), QDateTime(), QDateTime(), false});
        }
      endInsertRows ();
      return true;
    }
  return false;
}

QStringList FrequencyList_v2_101::impl::mimeTypes () const
{
  QStringList types;
  types << mime_type;
  return types;
}

QMimeData * FrequencyList_v2_101::impl::mimeData (QModelIndexList const& items) const
{
  QMimeData * mime_data = new QMimeData {};
  QByteArray encoded_data;
  QDataStream stream {&encoded_data, QIODevice::WriteOnly};

  Q_FOREACH (auto const& item, items)
    {
      if (item.isValid () && frequency_column == item.column ())
        {
          stream << frequency_list_.at (item.row ());
        }
    }

  mime_data->setData (mime_type, encoded_data);
  return mime_data;
}

auto FrequencyList_v2_101::const_iterator::operator * () const -> Item const&
{
  return parent_->frequency_list ().at(parent_->mapToSource (parent_->index (row_, 0)).row ());
}

auto FrequencyList_v2_101::const_iterator::operator -> () const -> Item const *
{
  return &parent_->frequency_list ().at(parent_->mapToSource (parent_->index (row_, 0)).row ());
}

bool FrequencyList_v2_101::const_iterator::operator != (const_iterator const& rhs) const
{
  return parent_ != rhs.parent_ || row_ != rhs.row_;
}

bool FrequencyList_v2_101::const_iterator::operator == (const_iterator const& rhs) const
{
  return parent_ == rhs.parent_ && row_ == rhs.row_;
}

auto FrequencyList_v2_101::const_iterator::operator ++ () -> const_iterator&
{
  ++row_;
  return *this;
}

auto FrequencyList_v2_101::begin () const -> const_iterator
{
  return const_iterator (this, 0);
}

auto FrequencyList_v2_101::end () const -> const_iterator
{
  return const_iterator (this, rowCount ());
}

auto FrequencyList_v2_101::find (Frequency f) const -> const_iterator
{
  int row {0};
  for (; row < rowCount (); ++row)
    {
      if (m_->frequency_list_[mapToSource (index (row, 0)).row ()].frequency_ == f)
        {
          break;
        }
    }
  return const_iterator (this, row);
}

auto FrequencyList_v2_101::filtered_bands () const -> BandSet
{
  BandSet result;
  for (auto const& item : *this)
    {
      result << m_->bands_->find (item.frequency_);
    }
  return result;
}

auto FrequencyList_v2_101::all_bands (Region region, Mode mode) const -> BandSet
{
  BandSet result;
  for (auto const& item : m_->frequency_list_)
    {
      // Match frequencies that are for all regions, for the specified
      // region (which can also be "all"), and where the mode matches
      // the specified mode (which can also be "all").
      if ((region == IARURegions::ALL || item.region_ == IARURegions::ALL || item.region_ == region)
          && (mode == Modes::ALL || item.mode_ == Modes::ALL || item.mode_ == mode))
        {
          result << m_->bands_->find (item.frequency_);
        }
    }
  return result;
}

FrequencyList_v2_101::FrequencyItems FrequencyList_v2_101::from_json_file(QFile *input_file)
{
  FrequencyList_v2_101::FrequencyItems list;
  QJsonDocument doc = QJsonDocument::fromJson(input_file->readAll());
  if (doc.isNull())
    {
      throw ReadFileException {tr ("Failed to parse JSON file")};
    }
  QJsonObject obj = doc.object();
  if (obj.isEmpty())
    {
      throw ReadFileException{tr("Information Missing")};
    }
  QJsonArray arr = obj["frequencies"].toArray();
  if (arr.isEmpty())
    {
      throw ReadFileException{tr ("No Frequencies were found")};
    }
#ifdef DUMP_ENTRY_COUNTS
  int valid_entry_count = 0;
  int skipped_entry_count = 0;
#endif
  for (auto const &item: arr)
    {
      QString mode_s, region_s;
      QJsonObject obj = item.toObject();
      FrequencyList_v2_101::Item freq;
      region_s = obj["region"].toString();
      mode_s = obj["mode"].toString();

      freq.frequency_ = obj["frequency"].toString().toDouble() * 1e6;
      freq.region_ = IARURegions::value(region_s);
      freq.mode_ = Modes::value(mode_s);
      freq.description_ = obj["description"].toString();
      freq.source_ = obj["source"].toString();
      freq.start_time_ = QDateTime::fromString(obj["start_time"].toString(), Qt::ISODate);
      freq.end_time_ = QDateTime::fromString(obj["end_time"].toString(), Qt::ISODate);
      freq.preferred_ = obj["preferred"].toBool();

      if ((freq.mode_ != Modes::ALL || QString::compare("ALL", mode_s)) &&
          (freq.region_ != IARURegions::ALL || QString::compare("ALL", region_s, Qt::CaseInsensitive)) &&
          freq.isSane())
        {
          list.push_back(freq);
#ifdef DUMP_ENTRY_COUNTS          
          valid_entry_count++;
#endif          
        } else {
#ifdef DUMP_ENTRY_COUNTS            
        skipped_entry_count++;
#endif    
    	}
    }

#ifdef DUMP_ENTRY_COUNTS
  MessageBox::information_message(this, tr("Loaded Frequencies from %1").arg(file_name),
                                  tr("Entries Valid/Skipped %1").arg(QString::number(valid_entry_count) + "/" +
                                                                     QString::number(skipped_entry_count)));
#endif
  return list;
}

// write JSON format to a file
void FrequencyList_v2_101::to_json_file(QFile *output_file, QString magic_s, QString version_s,
                                                    FrequencyItems const &frequency_items)
{
  QJsonObject jobject{
          {"wsjtx_file",      "qrg"},
          {"wsjtx_version",   QCoreApplication::applicationVersion()+" "+revision()},
          {"generated_at",    QDateTime::currentDateTimeUtc ().toString (Qt::ISODate)},
          {"wsjtx_filetype",  magic_s},
          {"qrg_version",     version_s},
          {"frequency_count", frequency_items.count()}};

  QJsonArray array;
  for (auto &item: frequency_items)
    array.append(item.toJson());
  jobject["frequencies"] = array;

  QJsonDocument d = QJsonDocument(jobject);
  output_file->write(d.toJson());
}

// previous version 100 of the FrequencyList_v2 class
QDataStream& operator >> (QDataStream& is, FrequencyList_v2::Item& item)
{
  return is >> item.frequency_
            >> item.mode_
            >> item.region_;
}

QDataStream& operator << (QDataStream& os, FrequencyList_v2::Item const& item)
{
  return os << item.frequency_
            << item.mode_
            << item.region_;
}

//
// Obsolete version of FrequencyList no longer used but needed to
// allow loading and saving of old settings contents without damage
//
QDataStream& operator << (QDataStream& os, FrequencyList::Item const& item)
{
  return os << item.frequency_
            << item.mode_;
}

QDataStream& operator >> (QDataStream& is, FrequencyList::Item& item)
{
  return is >> item.frequency_
            >> item.mode_;
}
