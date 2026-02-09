// LeonardOS - HTTP/1.0 Client
// GET requests simples para download de conteúdo web

#ifndef __HTTP_H__
#define __HTTP_H__

#include "../common/types.h"
#include "net_config.h"

// ============================================================
// Constantes HTTP
// ============================================================
#define HTTP_PORT           80
#define HTTP_MAX_URL        256
#define HTTP_MAX_HOST       128
#define HTTP_MAX_PATH       128
#define HTTP_MAX_HEADERS    2048    // Buffer para response headers
#define HTTP_BODY_BUF_SIZE  8192    // Buffer para body (increased)
#define HTTP_MAX_REDIRECTS  5       // Máximo de redirecionamentos

// ============================================================
// Resultado de uma requisição HTTP
// ============================================================
typedef struct {
    int      status_code;                   // 200, 404, etc
    char     headers[HTTP_MAX_HEADERS];     // Headers da resposta
    uint16_t headers_len;
    uint8_t  body[HTTP_BODY_BUF_SIZE];      // Body da resposta
    uint16_t body_len;
    bool     success;                       // true se status 2xx
    int      content_length;                // Content-Length (-1 se desconhecido)
    bool     truncated;                     // true se body > buffer
    int      redirect_count;                // Número de redirecionamentos seguidos
    char     redirect_url[HTTP_MAX_URL];    // URL final após redirecionamentos
} http_response_t;

// ============================================================
// URL parseada
// ============================================================
typedef struct {
    char     host[HTTP_MAX_HOST];
    char     path[HTTP_MAX_PATH];
    uint16_t port;
} http_url_t;

// ============================================================
// API pública
// ============================================================

// Inicializa o módulo HTTP
void http_init(void);

// Parseia uma URL http://host[:port]/path
// Retorna true se URL válida
bool http_parse_url(const char *url, http_url_t *out);

// Faz um HTTP GET
// url: URL completa (http://...)
// response: struct para armazenar resultado
// Retorna true se request completou (mesmo com erro HTTP)
bool http_get(const char *url, http_response_t *response);

// Estatísticas HTTP
typedef struct {
    uint32_t requests_sent;
    uint32_t responses_ok;
    uint32_t responses_error;
    uint32_t connect_failed;
} http_stats_t;

http_stats_t http_get_stats(void);

#endif
