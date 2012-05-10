set enviroment LD_LIBRARY_PATH=/home/longda/disk/d/depot/myself/longda/trunk/lfutil
b initUtil
b CIni::Load
r

b CommStage::makeStage
b Conn::recvCallback
b Conn::sendCallback


b CTestStage::handleEvent
b main
set args -f test/client.ini
r
b CommStage::makeStage
b Conn::recvCallback
b Conn::sendCallback


b CTestStage::handleEvent
b main
set args -f test/server.ini
r
b CommStage::makeStage
b Conn::recvCallback
b Conn::sendCallback



