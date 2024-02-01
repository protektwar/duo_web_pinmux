#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "duo_pinmux.h"

#define PORT 8080
#define BUFFER_SIZE 104857600

#define TRUE 1
#define FALSE 0
void make_index_html();

//---> from here, required by Ubuntu on the board to compile
unsigned long __stack_chk_guard;

void __stack_chk_guard_setup(void)
{
     __stack_chk_guard = 0xBAAAAAAD;//provide some magic numbers
}

void __stack_chk_fail(void)
{
	printf("variable corrupted");

}// will be called when guard variable is corrupted
//<--- up to here

char *basename(char *path)
{
    char *base = strrchr(path, '/');
    return base ? base+1 : path;
}

const char *get_file_extension(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name) {
        return "";
    }
    return dot + 1;
}

const char *get_mime_type(const char *file_ext) {
    if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0) {
        return "text/html";
    } else if (strcasecmp(file_ext, "txt") == 0) {
        return "text/plain";
    } else if (strcasecmp(file_ext, "css") == 0) {
        return "text/css";
    } else if (strcasecmp(file_ext, "jpg") == 0 || strcasecmp(file_ext, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcasecmp(file_ext, "png") == 0) {
        return "image/png";
    } else {
        return "application/octet-stream";
    }
}

bool case_insensitive_compare(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) {
            return false;
        }
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

char *get_file_case_insensitive(const char *file_name) {
    DIR *dir = opendir(".");
    if (dir == NULL) {
        perror("opendir");
        return NULL;
    }

    struct dirent *entry;
    char *found_file_name = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (case_insensitive_compare(entry->d_name, file_name)) {
            found_file_name = entry->d_name;
            break;
        }
    }

    closedir(dir);
    return found_file_name;
}

char *url_decode(const char *src) {
    size_t src_len = strlen(src);
    char *decoded = malloc(src_len + 1);
    size_t decoded_len = 0;

    // decode %2x to hex
    for (size_t i = 0; i < src_len; i++) {
        if (src[i] == '%' && i + 2 < src_len) {
            int hex_val;
            sscanf(src + i + 1, "%2x", &hex_val);
            decoded[decoded_len++] = hex_val;
            i += 2;
        } else {
            decoded[decoded_len++] = src[i];
        }
    }

    // add null terminator
    decoded[decoded_len] = '\0';
    return decoded;
}

void build_http_response(const char *file_name, 
                        const char *file_ext, 
                        char *response, 
                        size_t *response_len) {
    // build HTTP header
    const char *mime_type = get_mime_type(file_ext);
    char *header = (char *)malloc(BUFFER_SIZE * sizeof(char));
    snprintf(header, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "\r\n",
             mime_type);
    // if file not exist, response is 404 Not Found
    int file_fd = open(file_name, O_RDONLY);
    if (file_fd == -1) {
        snprintf(response, BUFFER_SIZE,
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "\r\n"
                 "404 Not Found");
        *response_len = strlen(response);
        return;
    }

    // get file size for Content-Length
    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;

    // copy header to response buffer
    *response_len = 0;
    memcpy(response, header, strlen(header));
    *response_len += strlen(header);

    // copy file to response buffer
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, 
                            response + *response_len, 
                            BUFFER_SIZE - *response_len)) > 0) {
        *response_len += bytes_read;
    }

    free(header);
    close(file_fd);
}

void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
    char *buffers[100];
    char *buffer1;
    int buffers_size = -1;

    // receive request data from client and store into buffer
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
//       printf("buffer: \n%s",buffer);
       char * pch;
       pch = strtok (buffer,"\n\r");
       while (pch != NULL){
	 buffers_size ++;
	 buffer1 = (char *)malloc(500 * sizeof(char));
	 strcpy(buffer1, pch);
	 buffers[buffers_size] = buffer1;
         pch = strtok (NULL, "\n\r");
       }

       char *url_encoded_file_name = (char*)malloc(100 * sizeof(char));
       bool is_POST = FALSE;
       for (int i = 0; i <= buffers_size; i++){
//	 printf("buffers[%d]: %s\n", i, buffers[i]);
	 if ((strstr(buffers[i], "GET") != NULL) || (strstr(buffers[i], "POST") != NULL)) {
		 if (strstr(buffers[i], "POST") != NULL)
			 is_POST = TRUE;
		 pch = strtok(buffers[i], " ");
		 int k=0;
		 while(pch != NULL){
		   if (k == 1){
	    		strcpy(url_encoded_file_name, pch);
		   }
		   pch = strtok(NULL, " ");
		   k++;
		 }
	 }
       }
       //printf("\n");
       if (is_POST == TRUE)
       {
	       char pin[10], func[30];
	       pch = strtok(buffers[buffers_size],"=");
	       pch = strtok(NULL, "-");
	       strcpy(pin, pch);
	       pch = strtok(NULL, "\n");
	       strcpy(func, pch);
	       if (change_pin_function(pin, func) == 0)
		       printf("pin/function error %s/%s\n", pin, func);
	       else
		       make_index_html();

       }
       free(buffer1);

       char *file_name = (char *)malloc(100 * sizeof(char));
       if (strlen(basename(url_decode(url_encoded_file_name)))==0){
         strcpy(file_name, "index.html");
       }
       else {
         strcpy(file_name, basename(url_decode(url_encoded_file_name)));
       }

       free(url_encoded_file_name);
            // get file extension
       char file_ext[32];
       strcpy(file_ext, get_file_extension(file_name));

            // build HTTP response
       char *response = (char *)malloc(BUFFER_SIZE * 2 * sizeof(char));
       size_t response_len;
       build_http_response(file_name, file_ext, response, &response_len);

            // send HTTP response to client
       send(client_fd, response, response_len, 0);

       free(response);
       free(file_name);
    }
    close(client_fd);
    free(arg);
    free(buffer);
    return NULL;
}

#include <signal.h>

static volatile int keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

void make_index_html()
{
  char* generated_html_code;

  FILE *fptr;
  fptr = fopen("index.html", "w");

  fprintf(fptr, "<html>\n");
  fprintf(fptr, "<head>\n");
  fprintf(fptr, "<title>\n");
  fprintf(fptr, "Web DUO pin multiplexing configuration\n");
  fprintf(fptr, "</title>\n");
  fprintf(fptr, "<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\">\n");
  fprintf(fptr, "<script type=\"text/javascript\">\n");
  fprintf(fptr, "function setElementById(variable,value) { \n"); 
  fprintf(fptr, "  var s = document.getElementById(variable);\n"); 
  fprintf(fptr, "  var v = document.getElementById(value).value;\n");
  fprintf(fptr, "  s.value = v; }\n");
  fprintf(fptr, "function submitMe(form) { var s = document.getElementById(form); s.submit(); }\n");
  fprintf(fptr, "</script>\n");
  fprintf(fptr, "</head>\n");
  fprintf(fptr, "<body>\n");
  fprintf(fptr, "<center><h1> Web DUO Pin multiplexing contigurator </h1></center>\n");
  fprintf(fptr, "<form id=\"form\" action=\"\" method=\"POST\">\n");
  fprintf(fptr, "<input type=\"hidden\" id=\"changeMe\" name=\"toChange\" value=\"\">\n");
  fprintf(fptr, "</form>\n");
  fprintf(fptr, "<table><tr>\n");
  fprintf(fptr, "<td id=\"td_left\" style=\"vertical-align: top;\">\n");
  generated_html_code = generate_html_code_pin("GP0", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP1", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GND", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP2", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP3", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP4", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP5", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GND", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP6", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP7", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP8", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP9", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GND", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP10", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP11", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP12", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP13", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GND", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP14", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP15", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);

  fprintf(fptr, "</td>");
  fprintf(fptr, "<td><img align=center src=\"duo_pinout_small.png\"></td>\n");
  fprintf(fptr, "<td id=\"td_right\" style=\"vertical-align: top;\">\n");
  generated_html_code = generate_html_code_pin("VBUS", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("VSYS", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GND", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("3V3_EN", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("3V3(OUT)", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("-", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("-", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GND", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP27", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP26", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("RUN", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP22", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GND", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP21", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP20", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP19", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP18", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GND", FALSE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP17", TRUE);
  fprintf(fptr, "%s", generated_html_code);
  free(generated_html_code);
  fprintf(fptr, "<br>\n");

  generated_html_code = generate_html_code_pin("GP16", TRUE);
  fprintf(fptr, "%s", generated_html_code);  
  free(generated_html_code);

  fprintf(fptr, "</td></tr></table>\n");
  fprintf(fptr, "</body>\n");
  fprintf(fptr, "</head>\n");
  fprintf(fptr, "</html>\n");

  fclose(fptr);
}
int main(int argc, char *argv[]) {
    int server_fd;
    struct sockaddr_in server_addr;

    make_index_html();

    //catch CTRL+C to exit, but only ofter a connection... To Be reviewd
    signal(SIGINT, intHandler);

    // create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // config socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // bind socket to port
    if (bind(server_fd, 
            (struct sockaddr *)&server_addr, 
            sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // listen for connections
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);
    while (keepRunning) {
        // client info
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        // accept client connection
        if ((*client_fd = accept(server_fd, 
                                (struct sockaddr *)&client_addr, 
                                &client_addr_len)) < 0) {
            perror("accept failed");
            continue;
        }

        // create a new thread to handle client request
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)client_fd);
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
