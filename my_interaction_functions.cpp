#include "my_interaction_functions.h"
#include <stdio.h>


extern "C" {
#include <FreeRTOS.h>
#include <task.h>
#include <interface.h>
}

int getBitValue(uInt8 value, uInt8 bit_n)
// given a byte value, returns the value of its bit n
{
    return(value & (1 << bit_n));
}

void setBitValue(uInt8* variable, int n_bit, int new_value_bit) 
//given a byte value, set the n bit to value
{
    uInt8 mask_on = (uInt8)(1 << n_bit);
    uInt8 mask_off = ~mask_on;
    if (new_value_bit)  *variable |= mask_on;
    else                *variable &= mask_off;

}

//Switch1 ON <=> bit 0 do porto 2 = 1
void switch1ON() {
    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 0, 1);
    writeDigitalU8(2, p);
}

//Switch1 OFF <=> bit 0 do porto 2 = 0
void switch1OFF() {
    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 0, 0);
    writeDigitalU8(2, p);
}

//Switch2 ON <=> bit 1 do porto 2 = 1
void switch2ON() {
    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 1, 1);
    writeDigitalU8(2, p);
}

//Switch2 OFF <=> bit 1 do porto 2 = 0
void switch2OFF() {
    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 1, 0);
    writeDigitalU8(2, p);
}

/***************************************************************************************************************
******************************* X axis related functions *******************************************************
***************************************************************************************************************/


//Mover o robo para a esquerda => bit 6(p2) = 1 & bit 7(p2) = 0
void moveXLeft() {
    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 6, 1);
    setBitValue(&p, 7, 0);
    writeDigitalU8(2, p);

}

//Mover o robo para a direita => bit 6(p2) = 0 & bit 7(p2) = 1
void moveXRight() {
    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 6, 0);
    setBitValue(&p, 7, 1);
    writeDigitalU8(2, p);

}

//Parar o eixo do X => bit 6(p2) = 0 & bit 7(p2) = 0
void stopX() {
    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 6, 0);
    setBitValue(&p, 7, 0);
    writeDigitalU8(2, p);

}

/*
* Output: 1- posicao 1 do X
*          2- posicao 2 do X
*           3- posicao 3 do X
*/
int  getXPos() {
    uInt8 p = readDigitalU8(0);
    if (!getBitValue(p, 2))
        return 1;
    if (!getBitValue(p, 1))
        return 2;
    if (!getBitValue(p, 0))
        return 3;
    return (-1);
    
}

/*
* Manda o robo ir para a posicao x 
*/
void gotoX(int x) {

    if (getXPos() == -1) {              //caso ainda nao esteja numa posicao de um sensor 

        moveXLeft();                    //movemos o robo para a esquerda
        
        while (1) {
            if (getXPos() == x) {       //ate que este encontre o sensor cuja posicao e dada pela variavel x
                stopX();                //quando encontrar entao paramos
                break;
            }

            if (getXPos() == 1) {       //caso o robo encontre a posicao 1 antes de encontrar a posicao do sensor x 
                moveXRight();           //significa que a posicao de destino se encontra a sua direita
            }
        }

    }
        
    //posicao de origem a esquerda da posicao destino
    if (getXPos() < x)      
        moveXRight();

    //posicao de origem a direita da posicao destino
    if (getXPos() > x) 
        moveXLeft();

    //quando chegar ao destino parar de mover
    while (1) {
        if (getXPos() == x) {
            stopX();
            break;
        }
    }

}


/***************************************************************************************************************
******************************* Y axis related functions *******************************************************
***************************************************************************************************************/

//Move o robo para dentro da cage <=> bit 5(p2) = 1 && bit 4(p2) = 0
void moveYInside() {
    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 5, 1);
    setBitValue(&p, 4, 0);
    writeDigitalU8(2, p);

}

//Move o robo para fora da cage <=> bit 5(p2) = 0 && bit 4(p2) = 1
void moveYOutside(){
    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 5, 0);
    setBitValue(&p, 4, 1);
    writeDigitalU8(2, p);

}

//Parar o eixo do Y => bit 5(p2) = 0 & bit 4(p2) = 0
void stopY(){
    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 5, 0);
    setBitValue(&p, 4, 0);
    writeDigitalU8(2, p);
}

/*
* Output: 1- posicao 1 do Y
*          2- posicao 2 do Y
*           3- posicao 3 do Y
*/
int  getYPos(){

    uInt8 p = readDigitalU8(0);
    if (!getBitValue(p, 5))
        return 1;
    if (!getBitValue(p, 4))
        return 2;
    if (!getBitValue(p, 3))
        return 3;
    return (-1);
}



void gotoY(int y) {
    
    if (getYPos() == -1) {                                  //caso ainda nao esteja numa posicao de um sensor 

        moveYInside();                                      //movemos o robo para dentro da cage

        while (1) {
            if (getYPos() == y) {                           //ate que este encontre o sensor cuja posicao e dada pela variavel y
                stopY();                                    //quando encontrar entao paramos
                break;
            }

            if (getYPos() == 3)                             //caso o robo encontre a posicao 3 antes de encontrar a posicao do sensor x 
                moveYOutside();                             //significa que a posicao de destino se encontra atras da posicao onde comecou
        }

    }

    //posicao de origem a esquerda da posicao destino
    if (getYPos() < y)
        moveYInside();

    //posicao de origem a direita da posicao destino
    if (getYPos() > y)
        moveYOutside();

    //quando chegar ao destino parar de mover

    while (1) {
        //printf("%d\n", getXPos());
        if (getYPos() == y) {
            stopY();
            break;
        }
    }

}




/***************************************************************************************************************
******************************* Z axis related functions *******************************************************
***************************************************************************************************************/

//Move o robo para cima => bit3(p2) = 1 && bit2(p2) = 0
void moveZUp() {

    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 3, 1);
    setBitValue(&p, 2, 0);
    writeDigitalU8(2, p);

}

//Move o robo para baixo => bit3(p2) = 0 && bit2(p2) =1
void moveZDown() {

    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 3, 0);
    setBitValue(&p, 2, 1);
    writeDigitalU8(2, p);

}

//Paramos o eixo Z do robo =>  bit3(p2) = 0 && bit2(p2) =0
void stopZ() {

    uInt8 p = readDigitalU8(2);
    setBitValue(&p, 3, 0);
    setBitValue(&p, 2, 0);
    writeDigitalU8(2, p);

}

/*
* Output: 1- posicao 1Down do Z 
*          2- posicao 2Down do Z
*           3- posicao 3Down do Z
*/
int  getZPos() {

    uInt8 p1 = readDigitalU8(1);
    uInt8 p0 = readDigitalU8(0);
    if (!getBitValue(p1, 3))
        return 1;
    if (!getBitValue(p1, 1))
        return 2;
    if (!getBitValue(p0, 7))
        return 3;
    return (-1);


}



void gotoZ(int z) {
   
    if (getZPos() == -1) {                                  //caso ainda nao esteja numa posicao de um sensor

        moveZUp();                                          //movemos o robo para cima

        while (1) {
            if (getZPos() == z) {                           //ate que este encontre o sensor cuja posicao e dada pela variavel z
                stopZ();                                    //quando encontrar entao paramos
                break;
            }

            if (getZPos() == 3)                             //caso o robo encontre a posicao 3 antes de encontrar a posicao do sensor z 
                moveZDown();                                //significa que a posicao de destino se encontrava abaixo da posicao onde comecou
        }

    }

    //posicao de origem a esquerda da posicao destino
    if (getZPos() < z)
        moveZUp();

    //posicao de origem a direita da posicao destino
    if (getZPos() > z)
        moveZDown();

    //quando chegar ao destino parar de mover

    while (1) {
        //printf("%d\n", getXPos());
        if (getZPos() == z) {
            stopZ();
            break;
        }
    }

}

//Coloca o robo no sensor UP da posicao onde ele ja se encontrava
void gotoZUp() {
    uInt8 p0 = readDigitalU8(0);
    uInt8 p1 = readDigitalU8(1);
    uInt8 p2 = readDigitalU8(2);
    
    //move up
    setBitValue(&p2, 3, 1);
    setBitValue(&p2, 2, 0);
    writeDigitalU8(2, p2);


    if(getZPos() == 1) {                   
        while(1) {
            p1 = readDigitalU8(1);
            if(!(getBitValue(p1, 2))) {
                stopZ();
                break;
            }
        }
    }

    if (getZPos() == 2) {
   
        while (1) {
            p1 = readDigitalU8(1);
            if (!(getBitValue(p1, 0))) {
                stopZ();
                break;
            }
        }
    }

    if (getZPos() == 3) {
        while (1) {
            p0 = readDigitalU8(0);
            if (!(getBitValue(p0, 6))) {
                stopZ();
                break;
            }
        }
    }

}

//Coloca o robo no sensorDown da po
void gotoZDown() {

    uInt8 p0 = readDigitalU8(0);
    uInt8 p1 = readDigitalU8(1);
    uInt8 p2 = readDigitalU8(2);

    //move down
    setBitValue(&p2, 3, 0);
    setBitValue(&p2, 2, 1);
    writeDigitalU8(2, p2);

    if (!(getBitValue(p1, 2))) {

        while (1) {
            p1 = readDigitalU8(1);
            if (!(getBitValue(p1, 3))) {
                stopZ();
                break;
            }
        }
    }

    if (!(getBitValue(p1, 0))) {

        while (1) {
            p1 = readDigitalU8(1);
            if (!(getBitValue(p1, 1))) {
                stopZ();
                break;
            }
        }
    }

    if (!(getBitValue(p0, 6))) {

        while (1) {
            p0 = readDigitalU8(0);
            if (!(getBitValue(p0, 7))) {
                stopZ();
                break;
            }
        }
    }
}

//UpInDownOut - sequencia que o robo tem que fazer para colocar um medicamento na celula
//Pré-Condiçao: ja estamos no down da célula onde vamos inserir a palete
void putPartInCell(){
    gotoZUp();
    gotoY(3);
    gotoZDown();
    gotoY(2);
}

//DownInUpOutDown - sequencia que o robo tem que fazer para retirar um medicamento na celula
//Pré-Condiçao: ja estamos no down da célula onde vamos inserir a palete portanto nao temos de fazer em primeiro lugar o down
void takePartInCell() {
    gotoY(3);
    gotoZUp();
    gotoY(2);
    gotoZDown();
}

/*Objetivo: verificar se a cage tem algum medicamento dentro dela <=> bit4(p1) = 1
* Return: TRUE - tem medicamento dentro da cage
*         FALSE - nao tem medicamento
*/
bool check_drug_cage() {
    uInt8 p1 = readDigitalU8(1);

    if (getBitValue(p1, 4))
        return TRUE;
    return FALSE;

}

//Objetivo: parar o robo
void emergency_stop() {
    uInt8 p2 = readDigitalU8(2);
    stopX();
    stopY();
    stopZ();
}

//Objetivo: Colocar no porto 2 os mesmos bits presentes na variavel enviada como paramentro, p22
void restaure_bits(uInt8 p22) {
    uInt8 p2 = readDigitalU8(2);

    for (int i = 0; i < 8; i++) {                   //ciclo responsavel por escrever cada um dos 8 bits
        setBitValue(&p2, i, getBitValue(p22, i));
    }

    writeDigitalU8(2, p2);
}




// put here all function's implementations