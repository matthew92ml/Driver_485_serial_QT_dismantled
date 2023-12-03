#include "singleinstanceapplication.h"

SingleInstanceApplication::SingleInstanceApplication(int argc, char *argv[]):QCoreApplication(argc, argv,true)
{
    singleAppSm = new QSharedMemory("singleAppGrdPrinterManager", this);
}


SingleInstanceApplication::~SingleInstanceApplication(){

    if(singleAppSm->isAttached()){
        qDebug()<<"singleAppSm->detach()!";
           singleAppSm->detach();
    }
}

bool SingleInstanceApplication::lock(){

    if(singleAppSm->attach(QSharedMemory::ReadOnly)){
            singleAppSm->detach();
            return false;
        }

        if(singleAppSm->create(1))
            return true;

        return false;
}
