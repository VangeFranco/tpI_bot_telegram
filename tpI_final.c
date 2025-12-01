#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <time.h>

struct memory {
  char *response;
  size_t size;
};

static size_t cb(char *data, size_t size, size_t nmemb, void *clientp)
{
	size_t realsize = nmemb;
	struct memory *mem = clientp;

	char *ptr = realloc(mem->response, mem->size + realsize + 1);
	if(!ptr) return 0; 

	mem->response = ptr;
	memcpy(&(mem->response[mem->size]), data, realsize);
	mem->size += realsize;
	mem->response[mem->size] = 0;

	return realsize;
}

void responder(CURL *curl, const char *token, long long chat_id, const char *msg, const char *usuario);
void registrar(const char *nombre, const char *mensaje, long fecha_unix);

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Uso: %s token.txt\n", argv[0]);
		return 1;
	}
	char token[256];
	FILE *tokenFile = fopen(argv[1], "r");
	
	if (!tokenFile){ 
		perror("Error. No se pudo abrir archivo"); 
		return 1; 
	}
	if (fgets(token, sizeof(token), tokenFile) == NULL) {
		printf("El archivo del token no se pudo leer o está vacío.\n");
		fclose(tokenFile);
		exit(1);
	}
	fclose(tokenFile);
	token[strcspn(token, "\n")] = 0;
	
	/*
	sprintf(telegramURL, "https://api.telegram.org/bot%s/getUpdates", token);
	
	CURLcode res;*/
	
	CURL *curl = curl_easy_init();
	if(!curl){
		printf("Error inicializado CURL\n");
		return 1;
	}
	struct memory chunk = {NULL, 0};
	
	long long last_update= 0;
	while (1) {
		if (chunk.response) free(chunk.response);
		chunk.response = NULL;
		chunk.size = 0;
		
		char url [512];
		sprintf(url, "https://api.telegram.org/bot%s/getUpdates?offset=%lld", token, last_update);
		
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); 
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
		CURLcode res = curl_easy_perform(curl);
		
		if (res == CURLE_OK && chunk.response) {
			printf("Respuesta de telegram:\n%s\n", chunk.response);
			char *ptr_id = strstr(chunk.response, "\"update_id\":");
			if (ptr_id) {
				long long update_id = 0;
				sscanf(ptr_id, "\"update_id\":%lld", &update_id);
				printf("update_id = %lld\n", update_id);
				
				if (update_id >= last_update) {
					last_update = update_id + 1;
					
					long long chat_id = 0;
					char nombre[128];
					char mensaje[256];
					long fecha = 0;
					
					char *chat_ptr = strstr(chunk.response, "\"chat\"");
					if(chat_ptr != NULL){
						char *id_ptr = strstr(chat_ptr, "\"id\":");
						if(id_ptr != NULL){
							id_ptr += strlen("\"id\":");
							sscanf(id_ptr, "%lld", &chat_id);
							printf("chat.id = %lld\n", chat_id);
						}
					}
					
					char *name_ptr = strstr(chunk.response, "\"first_name\"");
					if(name_ptr != NULL){
						name_ptr += strlen("\"first_name\":\"");
						sscanf(name_ptr, "%127[^\"]", nombre);
						printf("chat.first_name = %s\n",nombre);
					}
					
					char *text_ptr = strstr(chunk.response, "\"text\"");
					if(text_ptr != NULL){
						text_ptr += strlen("\"text\":\"");
						sscanf(text_ptr, "%255[^\"]", mensaje);
						printf("mensaje.text = %s\n",mensaje);
					}
					
					char *date_ptr = strstr(chunk.response, "\"date\":");
					if(date_ptr != NULL){
						date_ptr += strlen("\"date\":");
						sscanf(date_ptr, "%ld", &fecha);
					}	
					
					responder(curl, token, chat_id, mensaje, nombre);
					registrar(nombre, mensaje, fecha);
				}
			}
		}
		sleep(2);
	}
	curl_easy_cleanup(curl);
	return 0;
}


void responder(CURL *curl, const char *token, long long chat_id, const char *msg, const char *usuario){
	if(strlen(msg) == 0 || chat_id == 0) return;
	char texto[256];
	char reply_url[512];
	
	if(strcmp(msg, "hola") == 0 || strcmp(msg, "Hola") == 0){
		snprintf(texto, sizeof(texto), "Hola%%20%s%%2C%%20buen%%20dia", usuario);
	}else if(strcmp(msg, "chau") == 0 || strcmp(msg, "Chau") == 0){
		snprintf(texto, sizeof(texto), "Chau%%20%s%%2C%%20hasta%%20luego", usuario);
	}else{
		snprintf(texto, sizeof(texto), "No%%20entiendo%%20tu%%20mensaje.");
	}
	
	snprintf(reply_url, sizeof(reply_url), "https://api.telegram.org/bot%s/sendMessage?chat_id=%lld&text=%s", token, chat_id, texto);
	
	struct memory chunk2 = {NULL, 0};
	
	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_URL, reply_url);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk2);
	
	CURLcode res2 = curl_easy_perform(curl);
	
	if (res2 == CURLE_OK) {
		printf("Respuesta enviada a %s: %s\n", usuario, texto);
	}else {
		printf("Error. No se pudo enviar el mensaje (Código %d)\n", res2);
	}
	
	if(chunk2.response) free(chunk2.response);
}

void registrar(const char *nombre, const char *msg, long fecha_unix){
	FILE *file = fopen("registro.txt", "a");
	if (!file){
		perror("Error. No se pudo abrir el archivo.");
		return;
	}
	time_t now = fecha_unix;
	struct tm *info = localtime(&now);
	fprintf(file, "[%02d:%02d:%02d] %s: %s\n", info->tm_hour, info->tm_min, info->tm_sec, nombre, msg);
	fclose(file);
}
