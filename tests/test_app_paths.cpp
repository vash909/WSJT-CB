#include "qt_helpers.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QVariant>

#include <iostream>
#include <stdexcept>

#if defined (Q_OS_UNIX) && !defined (Q_OS_MACOS)
namespace
{
  QString const branded_name {"WSJT-CB by 1AT106 1XZ732 1AT1989"};
  QString const older_name {"WSJT-CB by 1AT106 - 1XZ732 - 161XZ085"};

  void require (bool condition, char const * message)
  {
    if (!condition) throw std::runtime_error {message};
  }

  void write_file (QString const& path, QByteArray const& contents)
  {
    require (QDir {}.mkpath (QFileInfo {path}.absolutePath ()), "Cannot create fixture directory");
    QFile file {path};
    require (file.open (QFile::WriteOnly), "Cannot open fixture file");
    require (file.write (contents) == contents.size (), "Cannot write fixture file");
  }

  QByteArray read_file (QString const& path)
  {
    QFile file {path};
    require (file.open (QFile::ReadOnly), "Cannot read expected file");
    return file.readAll ();
  }

  void set_instance (QString const& suffix)
  {
    QCoreApplication::setApplicationName (branded_name + suffix);
    QCoreApplication::instance ()->setProperty ("wsjtcb.settingsApplicationName", "WSJT-CB" + suffix);
  }
}

#endif

int main (int argc, char ** argv)
{
  QCoreApplication app {argc, argv};
#if !defined (Q_OS_UNIX) || defined (Q_OS_MACOS)
  // These fixtures redirect all standard locations through XDG variables.
  // Do not touch real AppData/Library folders on other platforms.
  std::cout << "Path fixtures require XDG standard locations\n";
  return 77;
#else
  try
    {
      QTemporaryDir sandbox;
      require (sandbox.isValid (), "Cannot create test sandbox");
      QDir root {sandbox.path ()};
      auto const data_root = root.filePath ("data");
      auto const config_root = root.filePath ("config");
      auto const system_config = root.filePath ("system-config");
      qputenv ("XDG_DATA_HOME", data_root.toUtf8 ());
      qputenv ("XDG_CONFIG_HOME", config_root.toUtf8 ());
      qputenv ("XDG_CACHE_HOME", root.filePath ("cache").toUtf8 ());
      qputenv ("XDG_CONFIG_DIRS", system_config.toUtf8 ());
      set_instance ({});

      auto const old_data = QDir {data_root}.filePath (branded_name);
      auto const older_data = QDir {data_root}.filePath (older_name);
      auto const new_data = QDir {data_root}.filePath ("WSJT-CB");
      write_file (old_data + "/save/old.wav", "audio");
      write_file (old_data + "/WSPR_history.txt", "old history");
      write_file (older_data + "/save/older.c2", "older audio");
      write_file (new_data + "/WSPR_history.txt", "current history");
      write_file (QDir {data_root}.filePath (branded_name + " - Radio B/foreign.txt"), "other rig");
      write_file (QDir {data_root}.filePath ("WSJT-CB - Radio B/stable-foreign.txt"), "other rig");

      require (wsjtcb_writable_location (QStandardPaths::DataLocation) == new_data, "Data must use the stable name");
      require (read_file (new_data + "/save/old.wav") == "audio", "Legacy audio was not copied");
      require (read_file (new_data + "/save/older.c2") == "older audio", "Older branding was not migrated");
      require (read_file (new_data + "/WSPR_history.txt") == "current history", "Migration overwrote current data");
      require (!QFileInfo::exists (new_data + "/foreign.txt"), "Migration mixed rig instances");
      require (!QFileInfo::exists (new_data + "/stable-foreign.txt"), "Migration copied another stable instance");
      require (QFileInfo::exists (old_data + "/save/old.wav"), "Migration removed the original audio");

      // Simulate SaveDir/MRUdir copied verbatim from an old INI file.
      require (wsjtcb_migrate_data_path (old_data + "/save") == new_data + "/save", "SaveDir was not rebased");
      require (wsjtcb_migrate_data_path (older_data + "/save/older.c2") == new_data + "/save/older.c2", "MRU path was not rebased");
      require (wsjtcb_migrate_data_path (old_data + "/save/../save") == new_data + "/save", "Path normalization failed");
      auto const custom = root.filePath ("custom/" + branded_name + "/save");
      require (wsjtcb_migrate_data_path (custom) == custom, "Custom save location was changed");
      require (wsjtcb_migrate_data_path (old_data + " backup/save") == old_data + " backup/save", "Partial folder name was matched");
      require (wsjtcb_migrate_data_path (QString {}).isEmpty (), "Empty saved path was changed");

      // The old directory may be removed after migration. New writes must work.
      require (QDir {old_data}.removeRecursively (), "Cannot remove legacy fixture");
      auto const save_dir = wsjtcb_migrate_data_path (old_data + "/save");
      require (QDir {save_dir}.exists (), "Migrated audio directory does not exist");
      write_file (save_dir + "/new.wav", "new audio");
      write_file (save_dir + "/new.c2", "new c2");
      write_file (wsjtcb_writable_location (QStandardPaths::DataLocation) + "/HRD Interface Information.txt", "HRD");
      write_file (wsjtcb_writable_location (QStandardPaths::DataLocation) + "/tx.wav", "TCI");
      require (!QDir {old_data}.exists (), "Runtime writes recreated the branded folder");

      auto const old_config = QDir {config_root}.filePath (branded_name);
      auto const new_config = QDir {config_root}.filePath ("WSJT-CB");
      write_file (old_config + "/hamlib_settings.json", "legacy Hamlib settings");
      auto const hamlib_path = wsjtcb_locate (QStandardPaths::AppConfigLocation, "hamlib_settings.json");
      require (hamlib_path == new_config + "/hamlib_settings.json", "Hamlib still uses the branded folder");
      require (read_file (hamlib_path) == "legacy Hamlib settings", "Hamlib configuration was not migrated");
      write_file (hamlib_path, "current Hamlib settings");
      require (read_file (wsjtcb_locate (QStandardPaths::AppConfigLocation, "hamlib_settings.json")) == "current Hamlib settings", "Legacy settings replaced current settings");
      require (wsjtcb_writable_location (QStandardPaths::ConfigLocation) == config_root, "Generic config location changed");
      require (!QFileInfo::exists (config_root + "/hamlib_settings.json"), "App data leaked into generic config location");
      write_file (system_config + "/WSJT-CB/system.json", "system settings");
      require (wsjtcb_locate (QStandardPaths::AppConfigLocation, "system.json") == system_config + "/WSJT-CB/system.json", "System configuration fallback was lost");
      require (wsjtcb_locate (QStandardPaths::AppConfigLocation, "missing.json").isEmpty (), "Missing lookup should be empty");

      set_instance (" - Radio B");
      auto const radio_data = new_data + " - Radio B";
      require (wsjtcb_writable_location (QStandardPaths::DataLocation) == radio_data, "Rig suffix was lost");
      require (read_file (radio_data + "/foreign.txt") == "other rig", "Named rig data was not migrated");
      require (!QFileInfo::exists (radio_data + "/WSPR_history.txt"), "Default-instance data leaked into named rig");
      require (wsjtcb_migrate_data_path (old_data + " - Radio B/save") == radio_data + "/save", "Named rig SaveDir was not rebased");
      require (wsjtcb_migrate_data_path (old_data + "/save") == old_data + "/save", "Another instance's explicit path was changed");
      set_instance (" - Radio B - test");
      auto const test_data = wsjtcb_writable_location (QStandardPaths::DataLocation);
      require (test_data == radio_data + " - test", "Test suffix was lost");
      require (!QFileInfo::exists (test_data + "/foreign.txt"), "Normal rig data leaked into test instance");
      std::cout << "Application paths, migration, audio writes and radio settings passed\n";
    }
  catch (std::exception const& e)
    {
      std::cerr << e.what () << '\n';
      return 1;
    }
  return 0;
#endif
}
