// LeonardOS - HTTP/1.1 Client
// GET requests via TCP, com Keep-Alive, Chunked Transfer, Redirect

#include "http.h"
#include "tcp.h"
#include "dns.h"
#include "arp.h"
#include "ethernet.h"
#include "net_config.h"
#include "../common/string.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../drivers/timer/pit.h"

// ============================================================
// Estatísticas
// ============================================================
static http_stats_t stats;

http_stats_t http_get_stats(void) {
    return stats;
}

// ============================================================
// Keep-Alive connection cache
// ============================================================
typedef struct {
    bool     active;
    char     host[HTTP_MAX_HOST];
    uint16_t port;
    int      conn_id;           // TCP connection id
    uint32_t last_use_ms;       // Timestamp do último uso
} http_keepalive_t;

static http_keepalive_t ka_cache[HTTP_KEEPALIVE_MAX];

#define HTTP_KEEPALIVE_TIMEOUT_MS 30000  // 30s idle timeout

// ============================================================
// Utilitários de string para HTTP
// ============================================================

// Converte inteiro para string decimal
static void int_to_str(int val, char *buf, int max) {
    if (max < 2) return;

    if (val < 0) {
        buf[0] = '-';
        buf[1] = '\0';
        return;
    }

    char tmp[12];
    int i = 0;

    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0 && i < 11) {
            tmp[i++] = '0' + (val % 10);
            val /= 10;
        }
    }

    // Reverte
    int j = 0;
    while (i > 0 && j < max - 1) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

// Parse de inteiro a partir de string
static int str_to_int(const char *s) {
    int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

// Busca case-insensitive de header no response
static const char *http_find_header(const char *headers, const char *name) {
    int name_len = kstrlen(name);
    const char *p = headers;

    while (*p) {
        // Compara case-insensitive
        bool match = true;
        for (int i = 0; i < name_len; i++) {
            if (ktolower(p[i]) != ktolower(name[i])) {
                match = false;
                break;
            }
        }

        if (match && p[name_len] == ':') {
            // Pula ": " e espaços
            p += name_len + 1;
            while (*p == ' ') p++;
            return p;
        }

        // Avança para próxima linha
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    return 0;
}

// Compara header value case-insensitive (até \r ou \n)
static bool http_header_contains(const char *header_val, const char *needle) {
    if (!header_val || !needle) return false;
    int nlen = kstrlen(needle);

    while (*header_val && *header_val != '\r' && *header_val != '\n') {
        bool match = true;
        for (int i = 0; i < nlen; i++) {
            if (ktolower(header_val[i]) != ktolower(needle[i])) {
                match = false;
                break;
            }
        }
        if (match) return true;
        header_val++;
    }
    return false;
}

// ============================================================
// Keep-Alive: busca conexão cached para host:port
// ============================================================
static int ka_find(const char *host, uint16_t port) {
    uint32_t now = pit_get_ms();

    for (int i = 0; i < HTTP_KEEPALIVE_MAX; i++) {
        if (!ka_cache[i].active) continue;

        // Verifica timeout
        if ((now - ka_cache[i].last_use_ms) > HTTP_KEEPALIVE_TIMEOUT_MS) {
            tcp_close(ka_cache[i].conn_id);
            ka_cache[i].active = false;
            continue;
        }

        // Verifica se conexão TCP ainda está viva
        if (!tcp_is_connected(ka_cache[i].conn_id)) {
            ka_cache[i].active = false;
            continue;
        }

        // Match host e port
        if (kstrcmp(ka_cache[i].host, host) == 0 && ka_cache[i].port == port) {
            ka_cache[i].last_use_ms = now;
            stats.keepalive_reuse++;
            return ka_cache[i].conn_id;
        }
    }
    return -1;  // Não encontrado
}

// Salva conexão no cache keep-alive
static void ka_store(const char *host, uint16_t port, int conn_id) {
    // Procura slot livre ou mais antigo
    int oldest = 0;
    uint32_t oldest_time = 0xFFFFFFFF;

    for (int i = 0; i < HTTP_KEEPALIVE_MAX; i++) {
        if (!ka_cache[i].active) {
            oldest = i;
            break;
        }
        if (ka_cache[i].last_use_ms < oldest_time) {
            oldest_time = ka_cache[i].last_use_ms;
            oldest = i;
        }
    }

    // Fecha conexão antiga se ocupado
    if (ka_cache[oldest].active) {
        tcp_close(ka_cache[oldest].conn_id);
    }

    kstrcpy(ka_cache[oldest].host, host, HTTP_MAX_HOST);
    ka_cache[oldest].port = port;
    ka_cache[oldest].conn_id = conn_id;
    ka_cache[oldest].last_use_ms = pit_get_ms();
    ka_cache[oldest].active = true;
}

// Fecha todas as conexões keep-alive
void http_close_keepalive(void) {
    for (int i = 0; i < HTTP_KEEPALIVE_MAX; i++) {
        if (ka_cache[i].active) {
            tcp_close(ka_cache[i].conn_id);
            ka_cache[i].active = false;
        }
    }
}

// ============================================================
// Chunked Transfer Encoding decoder
// Decodifica body chunked in-place no buffer
// Retorna tamanho total do body decodificado
// ============================================================
static int http_decode_chunked(const uint8_t *src, int src_len,
                               uint8_t *dst, int dst_max) {
    int src_pos = 0;
    int dst_pos = 0;

    while (src_pos < src_len) {
        // Parse chunk size (hex)
        int chunk_size = 0;
        while (src_pos < src_len) {
            char c = (char)src[src_pos];
            if (c >= '0' && c <= '9') {
                chunk_size = chunk_size * 16 + (c - '0');
            } else if (c >= 'a' && c <= 'f') {
                chunk_size = chunk_size * 16 + (c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                chunk_size = chunk_size * 16 + (c - 'A' + 10);
            } else {
                break;
            }
            src_pos++;
        }

        // Pula \r\n após tamanho
        while (src_pos < src_len && (src[src_pos] == '\r' || src[src_pos] == '\n'))
            src_pos++;

        // Chunk de tamanho 0 = fim
        if (chunk_size == 0) break;

        // Copia dados do chunk
        int to_copy = chunk_size;
        if (dst_pos + to_copy > dst_max) {
            to_copy = dst_max - dst_pos;
        }
        if (to_copy > 0 && src_pos + to_copy <= src_len) {
            kmemcpy(dst + dst_pos, src + src_pos, (uint16_t)to_copy);
            dst_pos += to_copy;
        }
        src_pos += chunk_size;

        // Pula \r\n após dados do chunk
        while (src_pos < src_len && (src[src_pos] == '\r' || src[src_pos] == '\n'))
            src_pos++;
    }

    return dst_pos;
}

// ============================================================
// http_parse_url — parseia http://host[:port]/path
// ============================================================
bool http_parse_url(const char *url, http_url_t *out) {
    if (!url || !out) return false;

    kmemset(out, 0, sizeof(http_url_t));
    out->port = HTTP_PORT;

    // Verifica prefixo http://
    if (kstrncmp(url, "http://", 7) != 0) return false;
    url += 7;

    // Extrai host (até : ou / ou fim)
    int i = 0;
    while (url[i] && url[i] != ':' && url[i] != '/' && i < HTTP_MAX_HOST - 1) {
        out->host[i] = url[i];
        i++;
    }
    out->host[i] = '\0';

    if (i == 0) return false; // Host vazio

    url += i;

    // Porta opcional
    if (*url == ':') {
        url++;
        out->port = (uint16_t)str_to_int(url);
        if (out->port == 0) out->port = HTTP_PORT;
        while (*url >= '0' && *url <= '9') url++;
    }

    // Path (default: "/")
    if (*url == '/') {
        kstrcpy(out->path, url, HTTP_MAX_PATH);
    } else {
        kstrcpy(out->path, "/", HTTP_MAX_PATH);
    }

    return true;
}

// ============================================================
// http_do_request — faz um HTTP/1.1 GET para uma URL já parseada
// Função interna usada por http_get (com suporte a redirect)
// ============================================================
static bool http_do_request(const http_url_t *parsed, http_response_t *response,
                            http_progress_fn progress) {
    // Resolve hostname para IP
    ip_addr_t server_ip;
    if (!dns_resolve(parsed->host, &server_ip)) {
        stats.connect_failed++;
        return false;
    }

    // Tenta reutilizar conexão keep-alive
    int conn = ka_find(parsed->host, parsed->port);
    bool reused = (conn >= 0);

    if (!reused) {
        // ARP pre-resolve (para gateway)
        {
            net_config_t *cfg = net_get_config();
            uint8_t dummy[6];
            arp_resolve(cfg->gateway, dummy);
            pit_sleep_ms(50);
            arp_resolve(cfg->gateway, dummy);
        }

        // Conecta via TCP
        conn = tcp_connect(server_ip, parsed->port, 5000);
        if (conn < 0) {
            stats.connect_failed++;
            return false;
        }
    }

    stats.requests_sent++;

    // Monta request HTTP/1.1
    static char request[512];
    int pos = 0;

    // GET /path HTTP/1.1\r\n
    const char *get = "GET ";
    for (int j = 0; get[j]; j++) request[pos++] = get[j];
    for (int j = 0; parsed->path[j]; j++) request[pos++] = parsed->path[j];
    const char *ver = " HTTP/1.1\r\n";
    for (int j = 0; ver[j]; j++) request[pos++] = ver[j];

    // Host: hostname\r\n (obrigatório em HTTP/1.1)
    const char *host_h = "Host: ";
    for (int j = 0; host_h[j]; j++) request[pos++] = host_h[j];
    for (int j = 0; parsed->host[j]; j++) request[pos++] = parsed->host[j];
    request[pos++] = '\r'; request[pos++] = '\n';

    // User-Agent
    const char *ua = "User-Agent: LeonardOS/1.0.0\r\n";
    for (int j = 0; ua[j]; j++) request[pos++] = ua[j];

    // Accept encoding (no compression, we can't decompress)
    const char *ae = "Accept-Encoding: identity\r\n";
    for (int j = 0; ae[j]; j++) request[pos++] = ae[j];

    // Connection: keep-alive
    const char *cc = "Connection: keep-alive\r\n";
    for (int j = 0; cc[j]; j++) request[pos++] = cc[j];

    // End of headers
    request[pos++] = '\r'; request[pos++] = '\n';
    request[pos] = '\0';

    // Envia request
    int sent = tcp_send(conn, request, (uint16_t)pos);
    if (sent < 0) {
        // Se conexão reutilizada morreu, tenta nova
        if (reused) {
            tcp_close(conn);
            conn = tcp_connect(server_ip, parsed->port, 5000);
            if (conn < 0) {
                stats.connect_failed++;
                return false;
            }
            reused = false;
            sent = tcp_send(conn, request, (uint16_t)pos);
            if (sent < 0) {
                tcp_close(conn);
                stats.responses_error++;
                return false;
            }
        } else {
            tcp_close(conn);
            stats.responses_error++;
            return false;
        }
    }

    // Recebe response (headers + body)
    static uint8_t raw_buf[HTTP_MAX_HEADERS + HTTP_BODY_BUF_SIZE];
    int total_received = 0;
    int max_raw = (int)sizeof(raw_buf) - 1;

    // Fase 1: Receber headers (até \r\n\r\n)
    int header_end = -1;
    while (total_received < max_raw && header_end < 0) {
        uint16_t chunk_size = (uint16_t)(max_raw - total_received);
        if (chunk_size > 4096) chunk_size = 4096;

        int chunk = tcp_recv(conn, raw_buf + total_received, chunk_size, 3000);
        if (chunk < 0) break;
        if (chunk == 0) break;
        total_received += chunk;

        // Procura fim dos headers
        for (int j = 0; j < total_received - 3; j++) {
            if (raw_buf[j] == '\r' && raw_buf[j+1] == '\n' &&
                raw_buf[j+2] == '\r' && raw_buf[j+3] == '\n') {
                header_end = j;
                break;
            }
        }
    }

    if (header_end < 0) {
        if (!reused) tcp_close(conn);
        stats.responses_error++;
        return false;
    }

    // Copia headers
    uint16_t hlen = (uint16_t)header_end;
    if (hlen >= HTTP_MAX_HEADERS) hlen = HTTP_MAX_HEADERS - 1;
    kmemcpy(response->headers, raw_buf, hlen);
    response->headers[hlen] = '\0';
    response->headers_len = hlen;

    // Parse status code
    if (kstrncmp(response->headers, "HTTP/", 5) == 0) {
        const char *p = response->headers + 5;
        while (*p && *p != ' ') p++;
        if (*p == ' ') {
            p++;
            response->status_code = str_to_int(p);
        }
    }

    // Parse Content-Length
    const char *cl = http_find_header(response->headers, "content-length");
    if (cl) {
        response->content_length = str_to_int(cl);
    }

    // Parse Transfer-Encoding: chunked
    const char *te = http_find_header(response->headers, "transfer-encoding");
    if (te && http_header_contains(te, "chunked")) {
        response->chunked = true;
    }

    // Parse Connection: keep-alive / close
    const char *conn_hdr = http_find_header(response->headers, "connection");
    if (conn_hdr) {
        response->keep_alive = http_header_contains(conn_hdr, "keep-alive");
    } else {
        // HTTP/1.1 default é keep-alive
        response->keep_alive = true;
    }

    // Fase 2: Receber body
    int body_start = header_end + 4;
    int body_in_buf = total_received - body_start;

    // Determina quanto mais precisamos ler
    bool need_more = true;
    if (!response->chunked && response->content_length >= 0) {
        // Temos Content-Length: ler exatamente essa quantidade
        int remaining = response->content_length - body_in_buf;
        while (remaining > 0 && total_received < max_raw) {
            uint16_t chunk_size = (uint16_t)(max_raw - total_received);
            if (chunk_size > 4096) chunk_size = 4096;
            if ((int)chunk_size > remaining) chunk_size = (uint16_t)remaining;

            int chunk = tcp_recv(conn, raw_buf + total_received, chunk_size, 3000);
            if (chunk <= 0) break;
            total_received += chunk;
            remaining -= chunk;

            // Progress callback
            if (progress) {
                progress(total_received - body_start, response->content_length);
            }
        }
        need_more = false;
    }

    if (need_more) {
        // Sem Content-Length ou chunked: ler até fechar ou buffer cheio
        while (total_received < max_raw) {
            uint16_t chunk_size = (uint16_t)(max_raw - total_received);
            if (chunk_size > 4096) chunk_size = 4096;

            int chunk = tcp_recv(conn, raw_buf + total_received, chunk_size, 3000);
            if (chunk <= 0) break;
            total_received += chunk;

            if (progress) {
                progress(total_received - body_start, response->content_length);
            }

            if (tcp_peer_closed(conn)) break;

            // Para chunked: verifica se já temos o chunk final (0\r\n\r\n)
            if (response->chunked) {
                // Procura "0\r\n\r\n" no final do buffer
                int end = total_received;
                if (end >= 5 &&
                    raw_buf[end-5] == '0' && raw_buf[end-4] == '\r' &&
                    raw_buf[end-3] == '\n' && raw_buf[end-2] == '\r' &&
                    raw_buf[end-1] == '\n') {
                    break;
                }
                // Variante: "0\r\n\r\n" com body antes
                bool found_end = false;
                for (int j = body_start; j < end - 4; j++) {
                    if (raw_buf[j] == '\r' && raw_buf[j+1] == '\n' &&
                        raw_buf[j+2] == '0' && raw_buf[j+3] == '\r' &&
                        raw_buf[j+4] == '\n') {
                        found_end = true;
                        break;
                    }
                }
                if (found_end) break;
            }
        }
    }

    raw_buf[total_received] = '\0';

    // Copia body
    int body_available = total_received - body_start;
    if (body_available > 0) {
        if (response->chunked) {
            // Decodifica chunked transfer encoding
            int decoded = http_decode_chunked(raw_buf + body_start, body_available,
                                              response->body, HTTP_BODY_BUF_SIZE);
            response->body_len = (uint16_t)decoded;
            if (decoded >= HTTP_BODY_BUF_SIZE) response->truncated = true;
            stats.chunked_responses++;
        } else {
            uint16_t blen = (uint16_t)body_available;
            if (blen > HTTP_BODY_BUF_SIZE) {
                blen = HTTP_BODY_BUF_SIZE;
                response->truncated = true;
            }
            kmemcpy(response->body, raw_buf + body_start, blen);
            response->body_len = blen;
        }
    }

    response->success = (response->status_code >= 200 && response->status_code < 300);

    if (response->success) {
        stats.responses_ok++;
    } else {
        stats.responses_error++;
    }

    // Gerencia conexão: keep-alive ou close
    if (response->keep_alive && response->success) {
        ka_store(parsed->host, parsed->port, conn);
    } else {
        // Remove do cache se estava lá
        for (int i = 0; i < HTTP_KEEPALIVE_MAX; i++) {
            if (ka_cache[i].active && ka_cache[i].conn_id == conn) {
                ka_cache[i].active = false;
                break;
            }
        }
        tcp_close(conn);
    }

    return true;
}

// ============================================================
// http_extract_location — extrai URL do header Location
// ============================================================
static bool http_extract_location(const char *headers, const char *current_host,
                                  uint16_t current_port, char *out_url, int max) {
    const char *loc = http_find_header(headers, "location");
    if (!loc) return false;

    // Copia até \r ou \n ou fim
    int i = 0;
    while (loc[i] && loc[i] != '\r' && loc[i] != '\n' && i < max - 1) {
        out_url[i] = loc[i];
        i++;
    }
    out_url[i] = '\0';

    // Se URL é relativa (começa com /), constrói URL absoluta
    if (out_url[0] == '/') {
        char path[HTTP_MAX_PATH];
        kstrcpy(path, out_url, HTTP_MAX_PATH);

        int pos = 0;
        const char *prefix = "http://";
        for (int j = 0; prefix[j] && pos < max - 1; j++)
            out_url[pos++] = prefix[j];
        for (int j = 0; current_host[j] && pos < max - 1; j++)
            out_url[pos++] = current_host[j];
        if (current_port != 80) {
            out_url[pos++] = ':';
            // Simple port to string
            char port_str[8];
            int_to_str(current_port, port_str, sizeof(port_str));
            for (int j = 0; port_str[j] && pos < max - 1; j++)
                out_url[pos++] = port_str[j];
        }
        for (int j = 0; path[j] && pos < max - 1; j++)
            out_url[pos++] = path[j];
        out_url[pos] = '\0';
    }

    return out_url[0] != '\0';
}

// ============================================================
// http_get — faz um HTTP/1.1 GET com suporte a redirect
// ============================================================
bool http_get(const char *url, http_response_t *response) {
    return http_get_with_progress(url, response, 0);
}

// ============================================================
// http_get_with_progress — GET com callback de progresso
// ============================================================
bool http_get_with_progress(const char *url, http_response_t *response,
                            http_progress_fn progress) {
    if (!url || !response) return false;

    kmemset(response, 0, sizeof(http_response_t));
    response->content_length = -1;
    response->redirect_count = 0;

    // Copia URL atual para buffer de trabalho
    static char current_url[HTTP_MAX_URL];
    kstrcpy(current_url, url, HTTP_MAX_URL);

    for (int redirect = 0; redirect <= HTTP_MAX_REDIRECTS; redirect++) {
        // Parseia URL
        http_url_t parsed;
        if (!http_parse_url(current_url, &parsed)) {
            return false;
        }

        // Limpa response para esta tentativa (preserva redirect_count)
        int saved_redirects = response->redirect_count;
        kmemset(response, 0, sizeof(http_response_t));
        response->content_length = -1;
        response->redirect_count = saved_redirects;

        // Faz request
        if (!http_do_request(&parsed, response, progress)) {
            return false;
        }

        // Verifica se é redirect (301, 302, 303, 307, 308)
        if (response->status_code == 301 || response->status_code == 302 ||
            response->status_code == 303 || response->status_code == 307 ||
            response->status_code == 308) {

            if (redirect >= HTTP_MAX_REDIRECTS) {
                return true;
            }

            // Extrai Location header
            static char redir_url[HTTP_MAX_URL];
            if (!http_extract_location(response->headers, parsed.host,
                                       parsed.port, redir_url, HTTP_MAX_URL)) {
                return true;
            }

            response->redirect_count++;
            kstrcpy(current_url, redir_url, HTTP_MAX_URL);
            pit_sleep_ms(100);
            continue;
        }

        // Não é redirect — salva URL final
        kstrcpy(response->redirect_url, current_url, HTTP_MAX_URL);
        return true;
    }

    return true;
}

// ============================================================
// http_init
// ============================================================
void http_init(void) {
    kmemset(&stats, 0, sizeof(stats));
    kmemset(ka_cache, 0, sizeof(ka_cache));

    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("HTTP: client HTTP/1.1 pronto (keep-alive, chunked)\n", THEME_BOOT);
}
