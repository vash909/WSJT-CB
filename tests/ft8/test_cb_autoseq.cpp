#include <QCoreApplication>
#include <QStringList>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <iostream>
#include <stdexcept>

#include "Radio.hpp"
#include "Decoder/decodedtext.h"
#include "qt_helpers.hpp"

// UI/CAT substitutes only: the two decision methods below are extracted from
// widgets/mainwindow.cpp at build time, so this tests the production AutoSeq
// logic without starting audio, CAT, timers, networking, or a transmitter.
struct Control {
  QString text_ {"26AT101"};
  int value_ {1500};
  bool checked_ {true};
  bool enabled_ {true};
  bool clicked_ {false};
  QString text() const { return text_; }
  int value() const { return value_; }
  bool isVisible() const { return true; }
  bool isEnabled() const { return enabled_; }
  bool isChecked() const { return checked_; }
  void click() { clicked_ = true; }
};
struct TestUI {
  Control dx,rx,tx,txOne,autoSeq,stop;
  Control *dxCallEntry {&dx}, *RxFreqSpinBox {&rx}, *TxFreqSpinBox {&tx};
  Control *tx1 {&txOne}, *cbAutoSeq {&autoSeq}, *stopTxButton {&stop};
};
struct TestConfig { QString my_callsign() const { return "1AT106"; } };
enum class SpecOp { NONE, FOX, HOUND };
#define LOG_INFO(x) ((void)0)
class MainWindow {
public:
  enum Progress { CALLING, REPLYING, REPORT, ROGER_REPORT, ROGERS, SIGNOFF };
  TestConfig m_config;
  TestUI controls;
  TestUI *ui {&controls};
  QString m_baseCall {"1AT106"}, m_mode {"FT8"}, m_hisCall {"26AT101"}, m_xRcvd;
  bool m_bCallingCQ {false}, m_auto {true}, m_sentFirst73 {false}, m_bAutoReply {true};
  Progress m_QSOProgress {REPORT};
  SpecOp m_specOp {SpecOp::NONE};
  int accepted {0};
  QString accepted_message;
  bool has_complete_cb_peer(DecodedText const&) const;
  void auto_sequence(DecodedText const&, unsigned, unsigned);
  void processMessage(DecodedText const& message) { ++accepted; accepted_message = message.string(); }
};
#include "cb_autoseq_methods.inc"

static void require(bool ok, char const *message) {
  if (!ok) throw std::runtime_error(message);
}
static DecodedText decode(QString const& text) {
  return DecodedText {"120000 -12  0.0 1500 ~  " + text};
}
int main(int argc, char **argv) {
  QCoreApplication app(argc,argv);
  try {
    for (auto const& call : {"1AT106", "26AT101", "161XZ085", "001AB123", "1AT1000", "999ZZ/ZZ"})
      require(Radio::is_complete_cb_callsign(call), "valid CB callsign rejected");
    for (auto const& call : {"<...>", "26AT", "K1ABC", "26AT1000", "1AT?"})
      require(!Radio::is_complete_cb_callsign(call), "invalid or incomplete CB callsign accepted");
    require(Radio::cb_country_prefix("26AT101") == "026", "CB country prefix changed");
    for (auto const& text : {"<1AT106> 26AT101", "26AT101 1AT106", "<1AT106> 161XZ085"}) {
      MainWindow w;
      w.m_bCallingCQ = true;
      w.auto_sequence(decode(text),25,50);
      require(w.accepted == 1, "valid CB answer did not start AutoSeq");
    }
    for (auto const& text : {"<1AT106> <...>", "1AT106 26AT", "1AT106 K1ABC"}) {
      MainWindow w;
      w.m_bCallingCQ = true;
      w.auto_sequence(decode(text),25,50);
      require(w.accepted == 0, "unresolved/non-CB reply started a QSO");
    }
    for (auto const& text : {"1AT106 -12", "1AT106 R-12", "1AT106 RRR", "1AT106 RR73", "1AT106 73", "<1AT106> 26AT101 RR73"}) {
      MainWindow w;
      w.m_QSOProgress = MainWindow::ROGER_REPORT;
      auto const message = decode(text);
      require(message.frequencyOffset() == 1500 && message.snr() == -12, "decode columns changed");
      w.auto_sequence(message,25,50);
      require(w.accepted == 1, "CB report/final did not advance AutoSeq");
    }
    {
      MainWindow w;
      w.m_sentFirst73 = true;
      w.auto_sequence(decode("1AT106 RR73"),25,50);
      require(w.accepted == 0, "finished QSO restarted");
    }
    {
      MainWindow w;
      w.m_QSOProgress = MainWindow::REPLYING;
      w.controls.dx.text_ = "W9XYZ";
      w.auto_sequence(decode("K1ABC W9XYZ -12"),25,50);
      require(w.controls.stop.clicked_, "AutoSeq failed to stop when partner replies to another station");
    }
    // Integration runner can additionally supply real decoder output.
    if (argc == 2) {
      QFile file(argv[1]);
      require(file.open(QIODevice::ReadOnly|QIODevice::Text), "cannot open decoder output");
      QTextStream stream(&file);
      int checked = 0;
      while (!stream.atEnd()) {
        auto line = stream.readLine();
        if (!line.contains(" ~ ")) continue;
        DecodedText message(line);
        MainWindow w;
        w.m_QSOProgress = MainWindow::ROGER_REPORT;
        w.auto_sequence(message,25,50);
        require(w.accepted == 1, "real decoded CB message did not advance AutoSeq");
        ++checked;
      }
      require(checked > 0, "no real CB decodes were checked");
    }
    std::cout << "CB callsign, decoder columns and AutoSeq decisions passed\n";
  } catch (std::exception const& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
