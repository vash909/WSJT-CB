#include "about.h"

#include <QCoreApplication>
#include <QString>

#include "revision_utils.hpp"

#include "ui_about.h"

CAboutDlg::CAboutDlg(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::CAboutDlg)
{
  ui->setupUi(this);

  ui->labelTxt->setText ("<h2>" + product_versioned_name (revision ()) + "</h2>"

    "WSJT-CB is an optimized version of the WSJT software for<br />"
    "weak-signal CB 27MHz communications.  <br /><br />"
    "&copy; 2026 by Lorenzo 1AT106, Pietro 1XZ732,  <br />"
    "Mancausoft, 161XZ085. <br />"
    "WSJT-CB is based on the WSJT software <br />"
    "by Joe Taylor K1JT.<br /><br />"
    "We gratefully acknowledge contributions from:<br />"
    "The Alfa Tango DX Group for testing<br />"
    "The XZ Group for resources and developers<br />"
    "The 11m community for the support.<br />"
    "DISCLAIMER: This software is provided for educational purposes only.<br /><br />"
    "WSJT-CB is licensed under the terms of Version 3 <br />"
    "of the GNU General Public License (GPL) <br /><br />"
    "<a href=" TO_STRING__ (PROJECT_HOMEPAGE) ">"
    "<img src=\":/icon_128x128.png\" /></a>"
    "<a href=\"https://www.gnu.org/licenses/gpl-3.0.txt\">"
    "<img src=\":/gpl-v3-logo.png\" height=\"80\" /><br />"
    "https://www.gnu.org/licenses/gpl-3.0.txt</a>");
}

CAboutDlg::~CAboutDlg()
{
}
