/*
 ============================================================================
 Name        : Master.c
 Author      : Dario Poma
 Version     : 1.0
 Copyright   : Todos los derechos reservados papu
 Description : Proceso Master
 ============================================================================
 */

#include "Master.h"

int main(int argc,char** argv) {
//	if(argc!=5){
//		puts("mal numero de argumentos");
//		return -1;
//	}

	masterIniciar();

	scriptTransformacion = fopen(argv[1],"r+");
	scriptReductor = fopen(argv[2], "r+");

	mensajeEnviar(socketYAMA,Solicitud,argv[3],strlen(argv[3]));
	imprimirMensaje(archivoLog, "[MENSAJE] Solicitud enviada");


	masterAtender();

	masterFinalizar();


	return EXIT_SUCCESS;
}

//--------------------------------------- Funciones de Master -------------------------------------

void masterIniciar() {
	configuracionIniciar();
	establecerConexiones();
	senialAsignarFuncion(SIGINT, configuracionSenial);
	mutexIniciar(&errorBloque);
	mutexIniciar(&recepcionAlternativo); //necesito que arranque en 0 este
}



void masterFinalizar() {

}

bool masterActivado() {
	return estadoMaster == ACTIVADO;
}

bool masterDesactivado() {
	return estadoMaster == DESACTIVADO;
}

void masterActivar() {
	estadoMaster= ACTIVADO;
}

void masterDesactivar() {
	estadoMaster = DESACTIVADO;
}

//--------------------------------------- Funciones de Configuracion -------------------------------------

void configuracionIniciar() {
	configuracionIniciarLog();
	configuracionIniciarCampos();
	configuracion = configuracionCrear(RUTA_CONFIG, (Puntero)configuracionLeerArchivo, campos);
	configuracionImprimir(configuracion);
}

Configuracion* configuracionLeerArchivo(ArchivoConfig archivoConfig) {
	Configuracion* configuracion = memoriaAlocar(sizeof(Configuracion));
	stringCopiar(configuracion->ipYama, archivoConfigStringDe(archivoConfig, "IP_YAMA"));
	stringCopiar(configuracion->puertoYama, archivoConfigStringDe(archivoConfig, "PUERTO_YAMA"));
	archivoConfigDestruir(archivoConfig);
	return configuracion;
}

void configuracionImprimir(Configuracion* configuracion) {
	imprimirMensajeDos(archivoLog, "[CONEXION] Configuracion para conexion con YAMA (IP: %s | Puerto: %s)", configuracion->ipYama, configuracion->puertoYama);
}

void configuracionIniciarLog() {
	pantallaLimpiar();
	archivoLog = archivoLogCrear(RUTA_LOG, "Master");
	imprimirMensajeProceso("# PROCESO MASTER");
}

void configuracionIniciarCampos() {
	campos[0] = "IP_YAMA";
	campos[1] = "PUERTO_YAMA";
}

void configuracionSenial(int senial) {
	masterDesactivar();
	puts("");
}

//--------------------------------------- Funciones de algo -------------------------------------

void conectarConWorkerReduccionL(){

}




void reduccionLocal(Mensaje* m){
	WorkerReduccion* wr= deserializarReduccion(m);
	int i;
	Lista list = wr->tmps;
	pthread_t hilo;
	pthread_create(&hilo,NULL,&conectarConWorkerReduccionL, NULL);

	for(i=1;i<list_size(list);i++){

		if(strcmp(list_get(list,i) , list_get(list,i++))){//comparo los nombres de los temporales donde se guarda la reduccion (uno por nodo)
			//crear otro hilo
		}else{


		}

	}


}

void reduccionGlobal(Mensaje* mensaje){

}



void masterAtender(){

	Mensaje* mensaje=mensajeRecibir(socketYAMA);
	imprimirMensaje(archivoLog, "[MENSAJE] Lista de bloques recibida");
	Lista workers=list_create();
	int i;
	for(i=0;i<mensaje->header.tamanio;i+=DIRSIZE+INTSIZE*2+TEMPSIZE){
		WorkerTransformacion worker;
		memcpy(&worker.dir,mensaje->datos+i,DIRSIZE);
		memcpy(&worker.bloque,mensaje->datos+i+DIRSIZE,INTSIZE);
		memcpy(&worker.bytes,mensaje->datos+i+DIRSIZE+INTSIZE,INTSIZE);
		memcpy(&worker.temp,mensaje->datos+i+DIRSIZE+INTSIZE*2,TEMPSIZE);
		list_addM(workers,&worker,sizeof(WorkerTransformacion));
	}
	bool mismoNodo(Dir a,Dir b){
		return a.ip==b.ip&&a.port==b.port;//podría comparar solo ip
	}
	Lista listas=list_create();
	for(i=0;i<=workers->elements_count;i++){
		WorkerTransformacion* worker=list_get(workers,i);
		int j;
		for(j=0;j<=listas->elements_count;j++){
			Lista nodo=list_get(listas,j);
			WorkerTransformacion* cmp=list_get(nodo,0);
			if(mismoNodo(worker->dir,cmp->dir)){
				list_add(nodo,worker);
			}
		}
		Lista nodo=list_create();
		list_add(nodo, worker);
		list_add(listas,nodo);//creo que no hay que alocar nada
	}

	Lista hilos=list_create();
	for(i=0;i<=listas->elements_count;i++){
		pthread_t hilo;
		pthread_create(&hilo,NULL,(void*)establecerConexionConWorker,list_get(listas,i));
		list_addM(hilos,&hilo,sizeof(pthread_t));
	}

	while(masterActivado()){
		Mensaje* m = mensajeRecibir(socketYAMA);
		switch(m->header.operacion){
		case Aborto:
			imprimirMensaje(archivoLog,"[ABORTO] Abortando proceso");
	//		return EXIT_FAILURE; //supongo que los hilos mueren aca
			//si no se mueren matarlos
			break;
		case Cierre:
			imprimirMensaje(archivoLog,"[EJECUCION] Terminando proceso");
			masterDesactivar();
			break;
		case Transformacion://hubo un error y se recibió un bloque alternativo
			memcpy(&alternativo.bloque,mensaje->datos+i+DIRSIZE,INTSIZE);
			memcpy(&alternativo.bytes,mensaje->datos+i+DIRSIZE+INTSIZE,INTSIZE);
			memcpy(&alternativo.temp,mensaje->datos+i+DIRSIZE+INTSIZE*2,TEMPSIZE);
			mutexDesbloquear(&recepcionAlternativo);
			break;
		case ReducLocal:
			reduccionLocal(m);
			break;
		case ReducGlobal:
			reduccionGlobal(m);
			break;
		}
	}
	socketCerrar(socketYAMA);
	socketCerrar(socketWorker);
	archivoLogDestruir(archivoLog);
	memoriaLiberar(configuracion);
	fclose(scriptReductor);
	fclose(scriptTransformacion);
}


char* leerArchivo(FILE* archivo) {
	fseek(archivo, 0, SEEK_END);
	long posicion = ftell(archivo);
	fseek(archivo, 0, SEEK_SET);
	char *texto = malloc(posicion + 1);
	fread(texto, posicion, 1, archivo);
	texto[posicion] = '\0';
	return texto;
}

int archivoValido(FILE* archivo) {
	return archivo != NULL;
}

bool esUnArchivo(char* c) {
	return string_contains(c, ".");
}

void enviarArchivo(FILE* archivo) {
	char* texto = leerArchivo(archivo);
	fileCerrar(archivo);
	mensajeEnviar(socketYAMA, 4, texto, strlen(texto)+1);
	mensajeEnviar(socketWorker, 4, texto, strlen(texto)+1);
	free(texto);
}

char* leerCaracteresEntrantes() {
	int i, caracterLeido;
	char* cadena = malloc(1000);
	for(i = 0; (caracterLeido= getchar()) != '\n'; i++)
		cadena[i] = caracterLeido;
	cadena[i] = '\0';
	return cadena;
}

void establecerConexiones(){
	imprimirMensajeDos(archivoLog,"[CONEXION] Estableciendo Conexion con YAMA (IP: %s | Puerto %s)", configuracion->ipYama, configuracion->puertoYama);
	socketYAMA = socketCrearCliente(configuracion->ipYama, configuracion->puertoYama, ID_MASTER);
	imprimirMensaje(archivoLog, "[CONEXION] Conexion existosa con YAMA");

	//TODO se conecta con un worker al azar?
	//imprimirMensajeDos(archivoLog, "[CONEXION] Estableciendo Conexion con Worker (IP: %s | Puerto: %s)", configuracion->ipWorker, configuracion->puertoWorker);
	//socketWorker = socketCrearCliente(configuracion->ipWorker, configuracion->puertoWorker, ID_MASTER);
	imprimirMensaje(archivoLog, "[CONEXION] Conexion existosa con Worker");

}

void leerArchivoConfig(){

}

int hayWorkersParaConectar(){
	Mensaje* m = mensajeRecibir(socketYAMA);
	if(m->header.operacion==-1){
		imprimirMensaje(archivoLog,"No hay mas workers por conectar");
		return -1;
		}
	else{
		return 1;
		}

}

WorkerTransformacion* deserializarTransformacion(Mensaje* mensaje){
	int size = mensaje->header.tamanio;
	char* numeroip;
	int numeropuerto;
	int numeroBloque;
	int numeroBytes;
	char* nombretemporal;
	WorkerTransformacion* wt= malloc(size);

	memcpy(&numeroip, mensaje->datos,20);// habria que cambiarlo? porque el 20 esta harcodeado
	memcpy(&numeropuerto, mensaje->datos + 20, sizeof(int));
	memcpy(&numeroBloque, mensaje->datos + sizeof(int)+20 , sizeof(int));
	memcpy(&numeroBytes, mensaje->datos + sizeof(int)*2 + 20 , sizeof(int));
	memcpy(&nombretemporal, mensaje->datos + sizeof(int)*3 +12 +20, 12);//tamaño del nombre del temporal es 12

	strcpy(wt->dir.ip , numeroip);
	wt->dir.port = numeropuerto;
	wt->bloque = numeropuerto;
	wt->bytes = numeroBytes;
	strcpy(wt->temp, nombretemporal);

	return wt;

}

WorkerReduccion* deserializarReduccion(Mensaje* mensaje){
	int size = mensaje->header.tamanio;
	int tamanio_dirs;
	Lista direcciones;
	int tamaniolista;
	Lista temporales;
	int tamanionombrestemps;
	Lista nombretemps;
	WorkerReduccion* wr = malloc(size);

	memcpy(&tamanio_dirs, mensaje->datos, sizeof(int));
	memcpy(&direcciones,mensaje->datos + sizeof(int), tamanio_dirs);
	memcpy(&tamaniolista, mensaje->datos + tamanio_dirs + sizeof(int), sizeof(int));
	memcpy(&temporales, mensaje->datos + tamanio_dirs + sizeof(int)*2, tamaniolista);// no se si se puede esto
	memcpy(&tamanionombrestemps, mensaje->datos + tamanio_dirs + sizeof(int)*3, sizeof(int));
	memcpy(&nombretemps, mensaje->datos + tamanio_dirs + sizeof(int)*3 + tamanionombrestemps, tamanionombrestemps);


	wr->dirs_size = tamaniolista;
	wr->dirs = direcciones;
	wr->tmps_size = tamaniolista;
	wr->tmps = temporales;
	wr->nombretemp_size = tamanionombrestemps;
	wr-> nombretempsreduccion = nombretemps;

	return wr;

}



Lista workersAConectar(){
	Lista workers = list_create();
	int pos=0;
	Mensaje* mens;
	WorkerTransformacion* wt;
	while(hayWorkersParaConectar(socketYAMA)){
		mens=mensajeRecibir(socketYAMA);
		wt= deserializarTransformacion(mens);
		listaAgregarEnPosicion(workers,(WorkerTransformacion*)wt, pos);
		pos++;

	}

	return workers;
}


ListaSockets sockets(){
	Lista workers = workersAConectar();
	int size= listaCantidadElementos(workersAConectar());
	WorkerTransformacion* wt;
	int sWorker;
	ListaSockets sockets;
	int i;

	for(i=0; i<size;i++){

	 wt = listaObtenerElemento(workers,i);
	 sWorker = socketCrearCliente(wt->dir.ip,string_itoa(wt->dir.port),ID_MASTER);
	 listaSocketsAgregar(sWorker,&sockets);

	}

	return sockets;
}


void serializarYEnviar(Entero nroBloque, Entero nroBytes, char* nombretemp, Socket unSocket){
	int size_nombretemp = strlen(nombretemp)+1;
	char* data = malloc(sizeof(int)*2+size_nombretemp);
	int len = size_nombretemp+sizeof(int)*2;

	memcpy(data,&nroBloque, sizeof(int));
	memcpy(data+sizeof(int),&nroBytes, sizeof(int));
	memcpy(data+sizeof(int)*2,&nroBloque, sizeof(int));
	memcpy(data+(sizeof(int)*3),nombretemp,size_nombretemp);

	mensajeEnviar(unSocket,Transformacion,data,len);


}

void enviarScript(Socket unSocket, FILE* script, Entero operacion){//operacion para ver si es el script transformacion o reductor
	char* buffer = leerArchivo(script);
	int len;
	char* data;

	len = strlen(buffer); // ya esta el barra cero

	data = malloc(len + sizeof(int));

	memcpy(data, &len, sizeof(int));
	memcpy(data + sizeof(int),buffer, len);

	mensajeEnviar(unSocket,operacion, data, len);


}



void establecerConexionConWorker(Lista bloques){
	WorkerTransformacion* dir = list_get(bloques,0);
	socketWorker=socketCrearCliente(dir->dir.ip,string_itoa(dir->dir.port), ID_MASTER);
	enviarScript(socketWorker, scriptTransformacion, SCRIPT_TRANSFORMACION);
	enviarScript(socketWorker, scriptReductor, SCRIPT_REDUCTOR);
	int i;
	enviarBloques:
	for(i=0;i<bloques->elements_count;i++){
		//worker se debería bancar varios bloques, mirar
		WorkerTransformacion* wt=list_get(bloques,i);
		serializarYEnviar(wt->bloque,wt->bytes, wt->temp,socketWorker);
		imprimirMensajeDos(archivoLog,"[CONEXION] Estableciendo conexion con Worker (IP: %s | PUERTO: %d", wt->dir.ip, string_itoa(wt->dir.port));
	}
	Mensaje* mensaje = mensajeRecibir(socketWorker);
	//se recibe la direccion y el bloque, se lo mandas a yama
	if(mensaje->header.operacion==EXITO){
		imprimirMensajeUno(archivoLog, "[TRANSFORMACION] Transformacion realizada con exito en el Worker %i", &socketWorker); //desp lo cambio
		mensajeEnviar(socketYAMA,Transformacion,&socketWorker,sizeof(int));
	}else{
		mutexBloquear(&errorBloque);
		imprimirMensajeUno(archivoLog,"[TRANSFORMACION] Transformacion fallida en el Worker %i",&socketWorker);
		mensajeEnviar(socketYAMA,ERROR,&socketWorker,sizeof(int));
		mutexBloquear(&recepcionAlternativo);
		list_addM(bloques,&alternativo,sizeof alternativo);
		mutexDesbloquear(&errorBloque);
		//se podría modificar para que se puedan procesar varios errores
		//en paralelo, pero no vale la pena porque agrega mucho codigo
		//y se supone que estos errores son casos raros
		//para hacer eso se tendrian que sacar el semaforo y
		//enviar el numero de thread en el mensaje, para diferenciar despues
		goto enviarBloques;
	}
	mensajeEnviar(socketWorker,EXITO,NULL,0);
}

bool mismoNodo(Dir a,Dir b){
	return stringIguales(a.ip,b.ip) && stringIguales(a.port,b.port);
}








