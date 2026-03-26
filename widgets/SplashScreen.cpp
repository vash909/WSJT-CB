#include "SplashScreen.hpp"

#include <QPixmap>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QCoreApplication>

#include "revision_utils.hpp"
#include "pimpl_impl.hpp"

#include "moc_SplashScreen.cpp"

class SplashScreen::impl
{
public:
  impl ()
    : checkbox_ {"Do not show this again"}
  {
    main_layout_.addStretch ();
    main_layout_.addWidget (&checkbox_, 0, Qt::AlignRight);
  }

  QVBoxLayout main_layout_;
  QCheckBox checkbox_;
};

SplashScreen::SplashScreen ()
  : QSplashScreen {QPixmap {":/splash.png"}, Qt::WindowStaysOnTopHint}
{
  setLayout (&m_->main_layout_);
  showMessage (
    "<div style=\""
    "display:inline-block;"
    "min-width:700px;"
    "background-color:#220707;"
    "border:2px solid #8f1616;"
    "padding:14px 18px;"
    "color:#fff4e8;"
    "font-size:15pt;"
    "font-weight:700;"
    "text-align:center;"
    "\">"
    "<h2 style=\""
    "color:#ffd7d7;"
    "font-size:21pt;"
    "font-weight:900;"
    "margin:0 0 10px 0;"
    "\">" + product_versioned_name (revision ()) + "</h2>"
    "<span>This is WSJT-CB!</span><br /><br />"
    "<span>Check user guide and release notes.</span><br /><br />"
    "<span>Send issue reports to info@xzgroup.net or via GitHub</span><br />"
    "<span>An huge thank you to the supporters!</span><br /><br />"
    "<span style=\"color:#ffd7d7; font-weight:900;\">1AT771, Justin Land, 1AT023, 30MK44, 26AT043 and others.</span><br />"
    "<img src=\":/icon_128x128.png\" />"
    "<img src=\":/gpl-v3-logo.png\" height=\"80\" />"
    "</div>", Qt::AlignCenter);
  connect (&m_->checkbox_, &QCheckBox::stateChanged, [this] (int s) {
      if (Qt::Checked == s) Q_EMIT disabled ();
    });
}

SplashScreen::~SplashScreen ()
{
}
