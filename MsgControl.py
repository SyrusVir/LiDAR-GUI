from PyQt5 import QtCore as qtc
from PyQt5 import QtWidgets as qtw
from PyQt5 import QtGui as qtg

class MsgControl(qtw.QWidget):

    sig_send_msg = qtc.pyqtSignal(str)

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.send_button = qtw.QPushButton('SEND', self)
        self.send_button.setSizePolicy(qtw.QSizePolicy(qtw.QSizePolicy.Minimum,qtw.QSizePolicy.Minimum))
        self.send_lineedit = qtw.QLineEdit(self)
        self.rec_lineedit = qtw.QLineEdit(self)
        self.rec_lineedit.setEnabled(False)

        # palce lineedits together in form
        self.form_layout = qtw.QFormLayout()
        self.form_layout.addRow('Message',self.send_lineedit)
        self.form_layout.addRow('Received', self.rec_lineedit)

        # place form above button; 
        self.setLayout(qtw.QVBoxLayout()) #set main widget layout
        self.layout().addLayout(self.form_layout) #add lineedits
        self.layout().addWidget(self.send_button) #add button

        #connect signals
        self.send_button.released.connect(self.sendMsg)
        self.sig_send_msg.connect(self.setReceiveText) #DEBUG

        self.show()

    def sendMsg(self):
        self.sig_send_msg.emit(self.send_lineedit.text())

    def getSendSignal(self):
        return self.send_button.released

    def setReceiveText(self, text):
        self.rec_lineedit.setText(text)

if __name__ == "__main__":
    import sys
    app = qtw.QApplication(sys.argv)
    widg = MsgControl()
    sys.exit(app.exec_())