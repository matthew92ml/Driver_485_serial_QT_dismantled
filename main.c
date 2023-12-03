

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#include "modbus.h"

#define nDigitalIn 24
#define nAnalogIn 24
#define nDigitalOutControllino 40
#define nDigitalOutShj 16
#define nDigitalOutSeneca 16
#define nOutCommand 2
#define nWeights 6


struct region{
    uint binStatus[nDigitalIn];
    int outCommand[nDigitalOutControllino];     //40 deve essere il maggiore
    double voltageValue;    //per retrocompatibilità
    double aiValue[nAnalogIn];    //valori interi da convertire
};
struct region *rptr;

struct regionremotecmd{
    int outCommand[nOutCommand];
};

struct regionremotecmd *rptrcmd;

struct regionweight{
    double lastValueWeight[nWeights];
    double lastPointWeight[nWeights];
    int stableWeight[nWeights];
    int reloadParams;
};

struct regionweight_2{
    double lastValueWeight;
};

struct regionweight *rptrweight;
struct regionweight_2 *rptrweight_2;

char closeModbus = 0;
modbus_t *m_modbus;
double lastValueWeight[nWeights];
double lastValueWeight_2;
double lastValueVoltA;
uint lastValueDigitalInput = 0;
uint lastValueDigitalInput2 = 0;
uint binStatus[nDigitalIn];
uint16_t lastWeightUint16[6][2];
int fd; //descrittore shared memory
int fdcmd;
int fdweight;
int fdweight_2;


void modbusReadVoltmeter();
void modbusReadDigitalInputShj();
void modbusReadDigitalInputControllino();
void modbusWriteDigitalOutputSeneca();
void modbusWriteDigitalOutputShj();
void modbusWriteDigitalOutputControllino();
void modbusWriteDigitalOutputID(int chID);
void modbusReadWeight(int id, int slaveId);
void modbusReadWeight_2(int slaveId);
void writeDigitalInputTest(int chID);
void decodeDigitalInput();
void modbusWriteBaudRate();
void modbusTest();

void stampa();
int getSharedMemory();
int getSharedMemorycmd();
int getSharedMemory2();
int getSharedMemoryPeso2();
int simula = 0;
int stop = 0;
int test = 0;
int i,ii;
int DI_MODBUS_ADD;
int DO_MODBUS_ADD;
int DI_ADDRESS;
int DO_ADDRESS;
int DI_ADDRESS_2;
int AI_MODBUS_ADDRESS;
int AI_ADDRESS;
int WEIGHT_ADDRESS;
int WEIGHT_ADDRESS_2;
int configura = 0;
int abilitaPeso = 0;
int abilitaPeso_2 = 0;
int abilitaVolmetro = 0;
int debugOutput = 0;
int doSeneca = 0;
int doShj = 0;
int doControllino=0;
char input[20];
int inputmanual=0;
int testoutput = 0;
int testoutput1 = 0; //test tutte le uscite
int testinput = 0; //test singolo
int j = 0; //contatore usato per scegliere uscita singola
int stato=0; //stato del digitale
int chIDremoteCmd=15; //canale primo comando remoto o limite area di memoria dedicata ai cassonetti

int main(int argc, char *argv[])
{
    printf("modbusComm v14.0.0b (06/02/2020)\n");
    configura = 0;
    abilitaPeso=0;
    abilitaPeso_2=0;
    debugOutput = 0;
    abilitaVolmetro =0;
    doSeneca = 0;
    doControllino = 0;
    doShj = 0;
    test = 0;
    if(argc>1){
        if(strcmp(argv[1],"configura")==0) {
        configura = 1;
        printf("CONFIGURA\n");
        }
        else if(strcmp(argv[1],"test")==0) {
        test = 1;
        }
        else if(strcmp(argv[1],"testinput")==0){
           printf("TEST INPUT SINGOLO\n");
           printf("USCITA SINGOLA\n");
           testinput = 1;
           j=atoi(argv[2]);
           stato=atoi(argv[3]);
        }
        else if(strcmp(argv[1],"seneca")==0){
            //seneca
            printf("SENECA\n");
            doSeneca = 1;
            doControllino = 0;
            doShj = 0;
            DI_MODBUS_ADD = 3;
            DO_MODBUS_ADD = 4;
            DI_ADDRESS = 1;
            DO_ADDRESS = 0;
        }
        else if(strcmp(argv[1],"shj")==0){
            //shj
            printf("SHJ\n");
            doSeneca = 0;
            doControllino = 0;
            doShj = 1;
            DI_MODBUS_ADD = 254;
            DO_MODBUS_ADD = 254;
            DI_ADDRESS = 100;
            DI_ADDRESS_2 = 101;
            DO_ADDRESS = 102;
        }
        else if(strcmp(argv[1],"controllino")==0){
            //controllino
            printf("CONTROLLINO\n");
            doSeneca = 0;
            doShj = 0;
            doControllino = 1;
            DI_MODBUS_ADD = 247;
            DO_MODBUS_ADD = 247;
            AI_MODBUS_ADDRESS = 247;
            DI_ADDRESS = 0;
            AI_ADDRESS = 24;
            DO_ADDRESS = 16;
        }
     }
     if(argc>2){
         if(strcmp(argv[2],"mypin")==0){
            //MyPIN
            printf("MYPIN\n");
            WEIGHT_ADDRESS = 98;
            abilitaPeso = 1;
         }
         if(strcmp(argv[2],"atm05")==0){
            //MyPIN
            printf("ATM05\n");
            WEIGHT_ADDRESS_2 = 3;
            abilitaPeso_2 = 1;
         }
         if(strcmp(argv[2],"mypinatm05")==0){
            //MyPIN
            printf("MYPIN ATM05\n");
            WEIGHT_ADDRESS = 98;
            WEIGHT_ADDRESS_2 = 3;
            abilitaPeso = 1;
            abilitaPeso_2 = 1;
         }
         if(strcmp(argv[2],"stampa")==0){
            printf("STAMPA\n");
            debugOutput = 1;
         }
         if(strcmp(argv[2],"volt")==0){
            printf("VOLTMETRO\n");
            abilitaVolmetro = 1;
         }
         if(strcmp(argv[2],"testoutput")==0){
            printf("TEST OUTPUT\n");
            testoutput = 1;
         }
         //TODO
         if(strcmp(argv[2],"testoutput1")==0){
            printf("TEST OUTPUT TUTTI E 16\n");
            testoutput1 = 1;
         }
         if(strcmp(argv[2],"manual")==0){
            printf("Scrivere l'input da chiudere\n");
            inputmanual=1;
            testoutput = 1;
         }

     }
     if(argc>3){
         if(strcmp(argv[3],"stampa")==0){
           printf("STAMPA\n");
            debugOutput = 1;
         }
         if(strcmp(argv[3], "volt")==0){
             printf("VOLTMETRO\n");
            abilitaVolmetro = 1;
         }
     }
     if(argc>4){
         if(strcmp(argv[4], "stampa")==0){
            printf("STAMPA\n");
            debugOutput = 1;
         }
     }

    short modbusConnected = 0;
    if(configura==1)
         m_modbus = modbus_new_rtu("/dev/ttymxc2",19200,'N',8,1); //9600
    else
        m_modbus = modbus_new_rtu("/dev/ttymxc2",9600,'N',8,1); //9600

    int retsh = getSharedMemory();
    if(retsh<0){
        printf("modbusCom - Errore shared memory: %d\n",retsh);
        return -1;
    }
    else
        printf("modbusCom - Shared memory OK: %d\n",fd);

    getSharedMemorycmd();

    if(abilitaPeso){
        retsh = getSharedMemory2();
        if(retsh<0){
            printf("modbusCom - Errore shared memory weight: %d\n",retsh);
            return -1;
        }
        else
            printf("modbusCom - Shared memory weight OK: %d\n",fd);
    }

    if(abilitaPeso_2){
        retsh = getSharedMemoryPeso2();
        if(retsh<0){
            printf("modbusCom - Errore shared memory weight_2: %d\n",retsh);
            return -1;
        }
        else
            printf("modbusCom - Shared memory weight_2 OK: %d\n",fd);
    }

    //PRIMA DI INIZIALIZZAZIONI per non avere la memoria condivisa inizializzata ad ogni avvio
    if (testinput == 1){
              writeDigitalInputTest(j);
              return 0;
    }
    if(testoutput == 1 and inputmanual == 1){

        while (true) {
        printf("Ingressi da 0 a 23:\n");
        std::cin >> input;
        if(input=="exit")
        {
        std;
        }
        if(input=="close")
        {
            for(i = 0; i < nDigitalOutShj; i++){
                    rptr->outCommand[i] = 1;
                    printf("%d",rptr->outCommand[i]);
            }
        }
        else
        {
        writeDigitalInputTest((int)input);
        modbusWriteDigitalOutputID((int)input);
        }
        }
    }
    if(testoutput == 1 and inputmanual == 0){
        for(ii = 0; ii <11; ii++){
            modbusWriteDigitalOutputID(ii);
        }
        return 0;
    }
    //TODO
   if (testoutput1 == 1 and inputmanual == 0 ){
        for(ii = 0; ii <16; ii++){
            modbusWriteDigitalOutputID(ii);
            printf(ii);
        }
        return 0;
    }

    //inizializzazioni
    for(i = 0; i < nWeights; i++){
        lastValueWeight[i] = 0.0;
    }
    for(i = 0; i < nDigitalIn; i++){
        rptr->binStatus[i] = 0;
    }
    if(doControllino == 1){
        printf("RESET INIZIALE: ");
        for(i = 0; i < nDigitalOutControllino; i++){
                rptr->outCommand[i] = 0;
           }
        printf("\n");
    }
    else if(doSeneca == 1){
        printf("RESET INIZIALE: ");
        for(i = 0; i < nDigitalOutSeneca; i++){
                rptr->outCommand[i] = 0;
           }
        printf("\n");
    }
    else if(doShj == 1){
        printf("RESET INIZIALE: ");
        for(i = 0; i < nDigitalOutShj; i++){
                rptr->outCommand[i] = 1;
                printf("%d",rptr->outCommand[i]);
        }
        printf("\n");
    }
    /*
    for(i = 0; i < nOutCommand; i++){
        if(doSeneca == 1 || doControllino == 1)
            rptrcmd->outCommand[i] = 0;
        else
            rptrcmd->outCommand[i] = 0;       //parto da uno stato tutto CHIUSO
    }*/
    //rptr->outCommand[4] = 0;   //BLOCCO BIDONI

    while(!closeModbus){
        //connetto
        if(!modbusConnected){
            int retCon = modbus_connect(m_modbus);
            //FORZO IL FUNZIONAMENTO A 485
            //QUESTA OPERAZIONE VA FATTA DOPO L'APERTURA DELLA PORTA
            modbus_rtu_set_serial_mode(m_modbus,MODBUS_RTU_RS485);
            if(retCon == -1){
                printf("modbusCom - modbus non connesso\n");
                modbusConnected = 0;
            }
            else{
                printf("modbusCom - modbus connesso\n");
                modbusConnected = 1;
            }
        }
        else{
            if(configura==1){
               modbusWriteBaudRate();
                configura = 0;
                printf("CONFIGURAZIONE OK\n");
                usleep(10000);
                //modbusTest();
                return 0;
            }
            if(test==1){
                printf("TEST\n");
                test = 0;
                modbusTest();
                return 0;
            }

            //leggi digitali
            if(testoutput == 0) {
                if(doShj==1 || doSeneca==1){
                    modbusReadDigitalInputShj();
                }
                else if(doControllino == 1){
                    modbusReadDigitalInputControllino();
                }
            }

            //scrive uscite
            usleep(10000);
            if(doSeneca == 1){
                modbusWriteDigitalOutputSeneca();
            }
            else if(doControllino == 1){
                modbusWriteDigitalOutputControllino();
            }
            else if(doShj == 1){
                modbusWriteDigitalOutputShj();
            }


            //leggi peso
            if(abilitaPeso==1){
                usleep(10000);
                modbusReadWeight(0,6);
            }
            if(abilitaPeso_2==1){
                usleep(10000);
                modbusReadWeight_2(15);
            }
            if(abilitaVolmetro == 1){
               usleep(10000);
               modbusReadVoltmeter();
            }

            if(debugOutput==1) stampa();
        }
        usleep(10000); //10 msec
    }

    modbus_close(m_modbus);
    modbus_free(m_modbus);
    m_modbus = NULL;

    close(fd);
    close(fdweight);
    close(fdweight_2);

    return 0;
}

void modbusTest(){
    int ret = -1;

    uint8_t dest[2]; //setup memory for data
    uint16_t * dest16 = (uint16_t *) dest;
    memset(dest, 0, 2);

    uint8_t dest4[4]; //setup memory for data
    u_int32_t * dest32 = (u_int32_t *) dest4;
    memset(dest4, 0, 4);

    ret = modbus_set_slave(m_modbus, DI_MODBUS_ADD);
    if(ret != 0)printf("modbusCom - modbusTest set_slave ret = %d\n",ret);

    ret = modbus_read_registers(m_modbus,9,2,dest16);
    printf("modbusCom - modbusTest baudrate =%d %d %d\n",ret,(int)dest16[0],(int)dest16[1]);

    ret = modbus_read_registers(m_modbus,0,4,dest32);
    printf("modbusCom - modbusTest serialnumber =%d %d%d%d%d\n",ret,(int)dest32[0],(int)dest32[1],(int)dest32[2],(int)dest32[3]);

    memset(dest, 0, 2);
    ret = modbus_read_registers(m_modbus,4,2,dest16);
    printf("modbusCom - modbusTest firmware =%d %d%d\n",ret,(int)dest16[0],(int)dest16[1]);

    memset(dest, 0, 2);
    ret = modbus_read_registers(m_modbus,7,2,dest16);
    printf("modbusCom - modbusTest product model =%d %d%d\n",ret,(int)dest16[0],(int)dest16[1]);

}

void modbusWriteBaudRate(){
    int ret = -1;

    ret = modbus_set_slave(m_modbus, DI_MODBUS_ADD);
    if(ret != 0)printf("modbusCom - modbusWriteBaudRate set_slave ret = %d\n",ret);

    ret = modbus_write_register(m_modbus,9,96);

    printf("modbusCom - modbusWriteBaudRate modbus_write_registers ret = %d\n",ret);
}

void stampa(){
    int i;
    printf("modbusCom - digital = %d\n",lastValueDigitalInput);
    printf("modbusCom - digital2 = %d\n",lastValueDigitalInput2);
    for(i = 0; i < nDigitalIn; i++)
        printf("d%d = %d\n",i,binStatus[i] );


    if(abilitaPeso){
        printf("\n");
        printf("modbusCom - peso = %f\n",lastValueWeight[0]);
    }
    if(abilitaPeso_2){
        printf("\n");
        printf("modbusCom - peso = %f\n",lastValueWeight_2);
    }

    if(abilitaVolmetro){
        printf("\n");
        printf("modbusCom - volt = %f\n",lastValueVoltA);
    }
}

void modbusReadAnalogInput(){
    if(m_modbus == NULL) return;
    uint8_t dest[1024]; //setup memory for data
    uint16_t * dest16 = (uint16_t *) dest;
    memset(dest, 0, 1024);
    int ret = -1; //return value from read functions

    ret = modbus_set_slave(m_modbus, AI_MODBUS_ADDRESS);
    if(ret != 0)printf("modbusCom - AI set slave ret = %d\n",ret);

    ret = modbus_read_registers(m_modbus, AI_ADDRESS, 1, dest16);
    if(ret == 1){
        for(int i = 0; i < nAnalogIn; i++)
            rptr->aiValue[i]=(double)dest16[i];

    }
}

void modbusReadVoltmeter(){
    if(m_modbus == NULL) return;
    uint8_t dest[1024]; //setup memory for data
    uint16_t * dest16 = (uint16_t *) dest;
    memset(dest, 0, 1024);
    int ret = -1; //return value from read functions

    ret = modbus_set_slave(m_modbus, 100);
    if(ret != 0)printf("modbusCom - Voltmeter set slave ret = %d\n",ret);

    ret = modbus_read_registers(m_modbus, 37, 1, dest16);
    //printf("modbusReadVoltmeter modbus_read_registers ret = %d\n",ret);
    if(ret == 1){
        lastValueVoltA = (double)dest16[0]/100;
        //printf("modbusReadVoltmeter lastValueVoltA = %f\n",lastValueVoltA);

        rptr->voltageValue=lastValueVoltA;
    }
}

void modbusReadDigitalInputShj(){
    if(m_modbus == NULL) return;
    uint8_t dest[1024]; //setup memory for data
    uint16_t * dest16 = (uint16_t *) dest;
    memset(dest, 0, 1024);
    int ret = -1; //return value from read functions

    ret = modbus_set_slave(m_modbus, DI_MODBUS_ADD);
    if(ret != 0)printf("modbusCom - DI set slave ret = %d\n",ret);
    ret = modbus_read_registers(m_modbus, DI_ADDRESS, 1, dest16);
    if(ret == 1){
        lastValueDigitalInput = (uint)dest16[0];
        if(debugOutput==1){
            printf("DI[0]: %d\n", lastValueDigitalInput);
        }
    }
    else{
        printf("modbusCom - DI modbus_read_registers = %d\n",ret);
    }

    if(DI_ADDRESS_2 > -1){
        ret = modbus_read_registers(m_modbus, DI_ADDRESS_2, 1, dest16);
        if(ret == 1){
            lastValueDigitalInput2 = (uint)dest16[0];

            if(debugOutput==1){
                printf("DI[1]: %d\n", lastValueDigitalInput2);
            }
        }
        else{
            printf("modbusCom - DI modbus_read_registers = %d\n",ret);
        }
    }

    decodeDigitalInput();

}

void modbusReadDigitalInputControllino(){
    if(m_modbus == NULL) return;
    uint8_t dest[1024]; //setup memory for data
    uint16_t * dest16 = (uint16_t *) dest;
    memset(dest, 0, 1024);
    int ret = -1; //return value from read functions

    ret = modbus_set_slave(m_modbus, DI_MODBUS_ADD);
    if(ret != 0)printf("modbusCom - DI set slave ret = %d\n",ret);
    int nb = nDigitalIn;
    ret = modbus_read_registers(m_modbus, DI_ADDRESS, nb, dest16);
    //if(ret > 0){
    lastValueDigitalInput = 0;
    int i;
    if(debugOutput){
        printf("ret= %d, dest[16]\n",ret);
    }
    for(i=0;i<nb;i++){
        if(debugOutput == 1) printf("%d",dest16[i]);
        lastValueDigitalInput = lastValueDigitalInput+dest16[i] * pow(2,i);
        binStatus[i] = dest16[i];
        rptr->binStatus[i] = binStatus[i];
    }
    if(debugOutput==1)printf("\n");


    /*
    ret = modbus_read_registers(m_modbus, DI_ADDRESS+8, nb, dest16);
    //if(ret > 0){
    lastValueDigitalInput = 0;
    if(debugOutput){
        int i;
        printf("ret= %d, dest[16]\n",ret);
        for(i=0;i<nb;i++){
            printf("%d",dest16[i]);
            lastValueDigitalInput = lastValueDigitalInput+dest16[i] * pow(2,i);
            binStatus[i+nb] = dest16[i];
            rptr->binStatus[i+nb] = binStatus[i+nb];
        }
        printf("\n");
    }
    */
}

void modbusWriteDigitalOutputSeneca(){
    int ret = -1;
    int chID;
    int value;

    ret = modbus_set_slave(m_modbus, DO_MODBUS_ADD);
    if(ret != 0)printf("modbusCom - DO set slave ret = %d\n",ret);

    usleep(10000);
    for(chID = 0; chID < nDigitalOutSeneca; chID++){
        value = rptr->outCommand[chID];
        if(chID >= chIDremoteCmd) value = rptrcmd->outCommand[chID-chIDremoteCmd];
        ret = modbus_write_bit(m_modbus,DO_ADDRESS + chID,value);
        //usleep(10000);
    }
   //  printf("modbusWriteDigitalOutput ret = %d\n",ret);
}

void modbusWriteDigitalOutputControllino(){
    int ret = -1;
    int chID;
    int value;

    ret = modbus_set_slave(m_modbus, DO_MODBUS_ADD);
    if(ret != 0)printf("modbusCom - DO set slave ret = %d\n",ret);

    usleep(10000);
    for(chID = 0; chID < nDigitalOutControllino; chID++){
        value = rptr->outCommand[chID];
        ret = modbus_write_register(m_modbus,DO_ADDRESS + chID,value);
    }
}

void modbusWriteDigitalOutputShj(){
    int ret = -1;
    int chID;
    int value;

    ret = modbus_set_slave(m_modbus, DO_MODBUS_ADD);
    if(ret != 0)printf("modbusCom - DO set slave ret = %d\n",ret);

    usleep(10000);
    //LUKE:
    int valueInt = 0;
    if(debugOutput == 1)printf("SHJ DO: ");
    for(chID=0; chID <nDigitalOutShj;chID++){
        value = rptr->outCommand[chID];
        if(debugOutput == 1)printf("%d",rptr->outCommand[chID]);
        valueInt = valueInt + value * pow(2,chID);
    }
   if(debugOutput == 1) printf("\n");
    ret = modbus_write_register(m_modbus,DO_ADDRESS,valueInt);
}

void writeDigitalInputTest(int chID){
    rptr->binStatus[chID]=stato;
}

void modbusWriteDigitalOutputID(int chID){
    int ret = -1;

    ret = modbus_set_slave(m_modbus, DO_MODBUS_ADD);
    if(ret != 0)printf("modbusCom - DO set slave ret = %d\n",ret);

    usleep(10000);
    int valueInt = 0;
    valueInt = 65535 - pow(2,chID);
    ret = modbus_write_register(m_modbus,DO_ADDRESS,valueInt);
    printf("modbusWriteDigitalOutputID ret = %d\n",ret);
    sleep(1);

    ret = modbus_write_register(m_modbus,DO_ADDRESS,65535);

}


void decodeDigitalInput(){
    uint binStatusWord = lastValueDigitalInput;
    uint binStatusWord2 = lastValueDigitalInput2;
    int i;
    for(i = 0; i < 16; i++){
        binStatus[i] = ((binStatusWord & (1 << i)) == (1 << i)) ? 0: 1;
        rptr->binStatus[i] = binStatus[i];
    }
    for(i = 0; i < 8; i++){
        binStatus[16+i] = ((binStatusWord2 & (1 << i)) == (1 << i)) ? 0: 1;
        rptr->binStatus[16+i] = binStatus[16+i];
    }
    if(debugOutput == 1){
        printf("DI: ");
        for(int i = 0; i < nDigitalIn; i++){
            printf("%d",rptr->binStatus[i] );
        }
        printf("\n");
    }
}

void modbusReadWeight(int id,int slaveId){
    if(id > nWeights-1) return;
    if(m_modbus == NULL) return;
    uint8_t dest[1024]; //setup memory for data
    uint16_t * dest16 = (uint16_t *) dest;
    memset(dest, 0, 1024);
    int ret = -1; //return value from read functions

    ret = modbus_set_slave(m_modbus, slaveId);
    if(ret != 0)printf("modbusCom - AI set slave ret = %d\n",ret);
    ret = modbus_read_registers(m_modbus, WEIGHT_ADDRESS, 2, dest16);

    if(ret == 2){

        //printf("PESO LETTO: %d %d\n",dest16[0],dest16[1]);
        if(dest16[0] < 32768){
            lastValueWeight[id] = dest16[0] + dest16[1]/65536.0;
            lastWeightUint16[id][0] = dest16[0];
            lastWeightUint16[id][1] = dest16[1];
        }
        else{
            lastValueWeight[id] = (32768 - dest16[0]) - dest16[1]/65536.0;
            lastWeightUint16[id][0] =  0;
            lastWeightUint16[id][1] = 0;
        }
        rptrweight->lastValueWeight[id]  = lastValueWeight[id];

        //calcolo la deviazione standard per la stabilità
        //se questa è inferiore a 0.5

        /*
        weightSamplesAvg = (nWeightSamples * weightSamplesAvg + lastValueWeight[id])/(nWeightSamples + 1);
        nWeightSamples++;
        weightStdDev = (lastValueWeight[id] - weightSamplesAvg)*(lastValueWeight[id] - weightSamplesAvg)/nWeightSamples;
        if(weightStdDev >= 0 && weightStdDev < 0.5)rptrweight->stableWeight[id]  = 1;
        else rptrweight->stableWeight[id]  = 0;
        */

        rptrweight->stableWeight[id]  = 1;
    }


}

void modbusReadWeight_2(int slaveId){
    if(m_modbus == NULL) return;
    uint8_t dest[1024]; //setup memory for data
    uint16_t * dest16 = (uint16_t *) dest;
    memset(dest, 0, 1024);
    int ret = -1; //return value from read functions

    ret = modbus_set_slave(m_modbus, slaveId);
    if(ret != 0)
        printf("modbusCom - w2 set slave ret = %d\n",ret);


   // ret = modbus_read_registers(m_modbus, WEIGHT_ADDRESS_2, 2, dest16);
    //TODO TEST LETTURA 1 WORD
    ret = modbus_read_registers(m_modbus, WEIGHT_ADDRESS_2, 2, dest16);

    printf("PESO2 ret: %d\n",ret);

    if(ret == 2){

        //printf("PESO LETTO: %d %d\n",dest16[0],dest16[1]);
        if(dest16[0] < 32768){
            lastValueWeight_2 = dest16[0] + dest16[1]/65536.0;
        }
        else{
            lastValueWeight_2 = (32768 - dest16[0]) - dest16[1]/65536.0;
        }
        lastValueWeight_2 = lastValueWeight_2/100;
        rptrweight_2->lastValueWeight  = lastValueWeight_2;

        printf("PESO LETTO: %f \n",lastValueWeight_2);
    }


}

int getSharedMemory(){
    fd = shm_open("/modbus",O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); //apertura
    if(fd == -1) return -1;

    if(ftruncate(fd,sizeof(struct region)) == -1) return -2;

    rptr = mmap(NULL,sizeof(struct region),PROT_READ | PROT_WRITE, MAP_SHARED, fd,0);
    if(rptr == MAP_FAILED) return -3;

    return 0;
}

int getSharedMemorycmd(){
    fdcmd = shm_open("/remotecmd",O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); //apertura
    if(fdcmd == -1) return -1;

    if(ftruncate(fdcmd,sizeof(struct regionremotecmd)) == -1) return -2;

    rptrcmd = mmap(NULL,sizeof(struct regionremotecmd),PROT_READ | PROT_WRITE, MAP_SHARED, fdcmd,0);
    if(rptrcmd == MAP_FAILED) return -3;

    return 0;
}

int getSharedMemory2(){
    fdweight = shm_open("/weight",O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); //apertura
    if(fdweight == -1) return -1;

    if(ftruncate(fdweight,sizeof(struct regionweight)) == -1) return -2;

    rptrweight = mmap(NULL,sizeof(struct regionweight),PROT_READ | PROT_WRITE, MAP_SHARED, fdweight,0);
    if(rptrweight == MAP_FAILED) return -3;

    return 0;
}

int getSharedMemoryPeso2(){
    fdweight_2 = shm_open("/weight_2",O_CREAT | O_RDWR, S_IRUSR | S_IWUSR); //apertura
    if(fdweight_2 == -1) return -1;

    if(ftruncate(fdweight_2,sizeof(struct regionweight_2)) == -1) return -2;

    rptrweight_2 = mmap(NULL,sizeof(struct regionweight_2),PROT_READ | PROT_WRITE, MAP_SHARED, fdweight_2,0);
    if(rptrweight_2 == MAP_FAILED) return -3;

    return 0;
}
