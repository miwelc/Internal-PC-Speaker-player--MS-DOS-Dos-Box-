#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <string.h>

#define TAMBUFFER 8*1024*sizeof(char)
#define CUENTADEFAULT 54

void interrupt ( *punteroInt8Orig)();
unsigned short bufferEnUso = 0;
unsigned short cambioBuff = 0;
unsigned char *doblebuffer[2];
unsigned short posBuffer = 0;
unsigned char byte;
unsigned char amplitudes[256];
unsigned short cuenta;

void interrupt NuevaISR (__CPPARGS) {
	//Reproducir una muestra
	if(posBuffer == TAMBUFFER) {
		bufferEnUso = 1-bufferEnUso;
		posBuffer = 0;
		cambioBuff = 1;
	}
	byte = doblebuffer[bufferEnUso][posBuffer++];
	outportb(0x42, (int)amplitudes[byte]);


	//Fin de la interrupcion
	outportb(0x20, 0x20);
}

int main(int argc, char **argv) {
	FILE * audioFile;
	unsigned short PCM;
	unsigned short numCanales;
	short int bitsMuestra;
	long int frecuencia;
	unsigned short int terminado = 0;
	unsigned short int archivoWAV = 0;
	unsigned short int i;

   //Reservamos 8KB por buffer
	doblebuffer[0] = (unsigned char *) malloc(TAMBUFFER);
	doblebuffer[1] = (unsigned char *) malloc(TAMBUFFER);

	//Comprobamos argumentos
	switch(argc) {
		case 1:
			printf("Nº de parametros incorrecto: altavozw audioFile [Frecuencia]");
			return 1;
			break;
		case 2:
			cuenta = CUENTADEFAULT;
			break;
		case 3:
			cuenta = 1193180/atoi(argv[2]);
			break;
	}
	if('.' == argv[1][strlen(argv[1])-4] &&
		'w' == argv[1][strlen(argv[1])-3] &&
		'a' == argv[1][strlen(argv[1])-2] &&
		'v' == argv[1][strlen(argv[1])-1])
		archivoWAV = 1;

	//Abrimos el archivo en lectura-binario
	audioFile = fopen (argv[1],"rb");
	if (audioFile == NULL) {
		printf("Error al abrir archivo");
		return 1;
	}

	if(archivoWAV) {
		fseek(audioFile, 20, SEEK_SET);
		fread(&PCM, 1, sizeof(short int), audioFile);
		fread(&numCanales, 1, sizeof(short int), audioFile);
		fseek(audioFile, 34, SEEK_SET);
		fread(&bitsMuestra, 1, sizeof(short int), audioFile);
		if(PCM == 1 && numCanales == 1 && bitsMuestra == 8) {
			fseek(audioFile , 24, SEEK_SET);
			fread(&frecuencia, 1, sizeof(long int), audioFile);
			cuenta = 1193180/frecuencia;
			fseek(audioFile, 44, SEEK_SET);
		} else {
			printf("Error en archivo WAV");
			return 1;
		}
	}

	//Llenamos el doblebuffer
	fread(doblebuffer[0], 1, TAMBUFFER, audioFile);
	fread(doblebuffer[1], 1, TAMBUFFER, audioFile);

	//Calculamos amplitudes
	for(i = 0; i < 256; i++)
		amplitudes[i] = (i*cuenta/256) + 1;

	//Int 8
	//Almacenamos el vector original
	punteroInt8Orig = getvect(8);
	//Cambiamos al nuevo
	setvect(8, NuevaISR);

	//Temporizador 2 en modo 0, solo el byte bajo
	outportb(0x43, 0x90);
	//Temporizador 0 en modo 3, onda cuadrada
	outportb(0x43, 0x16);
	//Temporizador 0 valor de cuenta
	outportb(0x40, cuenta);

	//Activar altavoz poniendo a 1 los bit 0 y 1 del puerto B del 8255
	byte = inportb(0x61);
	outportb(0x61, byte|0x03);

	//Esperar a que finalize o a que se pulse una tecla
	while(peekb(0x40,0x1A) == peekb(0x40,0x1C) && !terminado) {
		//Si cambiamos de buffer rellenamos el viejo de nuevo
		if(cambioBuff) {
			if(!fread(doblebuffer[1-bufferEnUso], 1, TAMBUFFER, audioFile)) terminado = 1;
			cambioBuff = 0;
		}
	}

	//Desactivar altavoz poniendo a 0 los bit 0 y 1 del puerto B del 8255
	byte = inportb(0x61);
	outportb(0x61, byte&0xFC); /////////

	//Temporizador 0 valor de cuenta 0
	outportb(0x40, 0x00);

	//Restaurar vector ISR
	setvect(8, punteroInt8Orig);

	//Liberamos memoria
	free(doblebuffer[0]);
	free(doblebuffer[1]);

	//Cerramos fichero
	fclose(audioFile);

	return 0;
}