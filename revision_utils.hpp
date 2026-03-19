#ifndef REVISION_UTILS_HPP__
#define REVISION_UTILS_HPP__

#include <QString>

QString revision (QString const& svn_rev_string = QString {});
QString version (bool include_patch = true);
QString product_name ();
QString product_versioned_name (QString const& revision = QString {});
QString product_user_agent (QString const& revision = QString {});
QString program_title (QString const& revision = QString {});

#endif
