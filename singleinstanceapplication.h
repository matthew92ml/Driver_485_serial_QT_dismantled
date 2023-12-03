#ifndef SINGLEINSTANCEAPPLICATION_H
#define SINGLEINSTANCEAPPLICATION_H

#include <QObject>
#include <QCoreApplication>
#include <QSharedMemory>
#include <QDebug>

class SingleInstanceApplication : public QCoreApplication
{
    Q_OBJECT

public:
    SingleInstanceApplication(int argc, char *argv[]);
    ~SingleInstanceApplication();

    bool lock();

private:
    QSharedMemory *singleAppSm;
};

#endif // SINGLEINSTANCEAPPLICATION_H
