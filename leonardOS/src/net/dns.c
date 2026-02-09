// LeonardOS - DNS Resolver
// Resolve hostnames via UDP port 53
// Usa QEMU DNS forwarder em 10.0.2.3

#include "dns.h"
#include "udp.h"
#include "ethernet.h"
#include "net_config.h"
#include "../common/string.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

// ============================================================
// Cache DNS
// ============================================================
static dns_cache_entry_t cache[DNS_CACHE_SIZE];
static dns_stats_t stats;
static uint16_t dns_query_id = 1;

dns_stats_t dns_get_stats(void) {
    return stats;
}

void dns_cache_clear(void) {
    kmemset(cache, 0, sizeof(cache));
}

// ============================================================
// Cache: busca
// ============================================================
static bool dns_cache_lookup(const char *hostname, ip_addr_t *out_ip) {
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (cache[i].valid && kstrcmp(cache[i].hostname, hostname) == 0) {
            *out_ip = cache[i].ip;
            stats.cache_hits++;
            return true;
        }
    }
    return false;
}

// ============================================================
// Cache: armazena
// ============================================================
static void dns_cache_store(const char *hostname, ip_addr_t ip) {
    // Procura slot existente ou vazio
    int slot = -1;
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (!cache[i].valid) {
            slot = i;
            break;
        }
        if (kstrcmp(cache[i].hostname, hostname) == 0) {
            slot = i;
            break;
        }
    }

    // Se tudo cheio, sobrescreve o primeiro
    if (slot < 0) slot = 0;

    kstrcpy(cache[slot].hostname, hostname, DNS_MAX_NAME);
    cache[slot].ip    = ip;
    cache[slot].valid = true;
}

// ============================================================
// Codifica hostname em formato DNS (labels)
// "www.example.com" → "\3www\7example\3com\0"
// Retorna o tamanho escrito
// ============================================================
static int dns_encode_name(const char *hostname, uint8_t *buf, int buf_size) {
    int pos = 0;
    const char *p = hostname;

    while (*p && pos < buf_size - 2) {
        // Encontra o próximo '.' ou fim
        const char *dot = p;
        while (*dot && *dot != '.') dot++;

        int label_len = (int)(dot - p);
        if (label_len <= 0 || label_len > 63) return -1;
        if (pos + 1 + label_len >= buf_size) return -1;

        buf[pos++] = (uint8_t)label_len;
        for (int i = 0; i < label_len; i++) {
            buf[pos++] = (uint8_t)p[i];
        }

        p = dot;
        if (*p == '.') p++;
    }

    buf[pos++] = 0; // Termina com label vazio
    return pos;
}

// ============================================================
// Monta e envia query DNS (tipo A)
// ============================================================
static bool dns_send_query(const char *hostname, uint16_t query_id) {
    static uint8_t pkt[256];
    int pos = 0;

    // Header DNS
    dns_header_t *hdr = (dns_header_t *)pkt;
    hdr->id      = htons(query_id);
    hdr->flags   = htons(DNS_FLAG_RD); // Recursion Desired
    hdr->qdcount = htons(1);           // Uma query
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;
    pos = sizeof(dns_header_t);

    // Question: hostname codificado
    int name_len = dns_encode_name(hostname, pkt + pos, 256 - pos - 4);
    if (name_len < 0) return false;
    pos += name_len;

    // QTYPE = A (1)
    pkt[pos++] = 0;
    pkt[pos++] = DNS_TYPE_A;

    // QCLASS = IN (1)
    pkt[pos++] = 0;
    pkt[pos++] = DNS_CLASS_IN;

    // Envia via UDP para DNS server (configurado ou default 10.0.2.3:53)
    net_config_t *cfg = net_get_config();
    ip_addr_t dns_server = cfg->dns;

    stats.queries_sent++;

    // Usa porta local ephemeral (ex: 5353)
    return udp_send(dns_server, DNS_PORT, 5353 + (query_id & 0xFF), pkt, (uint16_t)pos);
}

// ============================================================
// Pula um nome DNS na resposta (com compressão de ponteiros)
// ============================================================
static int dns_skip_name(const uint8_t *buf, int pos, int buf_len) {
    while (pos < buf_len) {
        uint8_t label = buf[pos];

        if (label == 0) {
            return pos + 1; // Fim do nome
        }

        // Ponteiro de compressão (2 bytes)
        if ((label & 0xC0) == 0xC0) {
            return pos + 2;
        }

        // Label normal
        pos += 1 + label;
    }
    return -1; // Erro
}

// ============================================================
// Parseia resposta DNS, extrai IP do registro A
// ============================================================
static bool dns_parse_response(const uint8_t *buf, int buf_len,
                               uint16_t expected_id, ip_addr_t *out_ip) {
    if (buf_len < (int)sizeof(dns_header_t)) return false;

    const dns_header_t *hdr = (const dns_header_t *)buf;

    // Verifica ID
    if (ntohs(hdr->id) != expected_id) return false;

    // Verifica se é resposta (QR=1)
    uint16_t flags = ntohs(hdr->flags);
    if (!(flags & DNS_FLAG_QR)) return false;

    // Verifica RCODE (deve ser 0 = No Error)
    if ((flags & DNS_FLAG_RCODE) != 0) return false;

    uint16_t qdcount = ntohs(hdr->qdcount);
    uint16_t ancount = ntohs(hdr->ancount);

    if (ancount == 0) return false;

    // Pula o header
    int pos = sizeof(dns_header_t);

    // Pula as questions
    for (uint16_t i = 0; i < qdcount; i++) {
        pos = dns_skip_name(buf, pos, buf_len);
        if (pos < 0) return false;
        pos += 4; // QTYPE (2) + QCLASS (2)
        if (pos > buf_len) return false;
    }

    // Parseia answers, procura registro A
    for (uint16_t i = 0; i < ancount; i++) {
        pos = dns_skip_name(buf, pos, buf_len);
        if (pos < 0 || pos + 10 > buf_len) return false;

        uint16_t rtype  = (uint16_t)(buf[pos] << 8 | buf[pos + 1]);
        // uint16_t rclass = (uint16_t)(buf[pos+2] << 8 | buf[pos+3]);
        // uint32_t ttl = ...;
        uint16_t rdlen  = (uint16_t)(buf[pos + 8] << 8 | buf[pos + 9]);

        pos += 10; // TYPE(2) + CLASS(2) + TTL(4) + RDLENGTH(2)

        if (rtype == DNS_TYPE_A && rdlen == 4) {
            // Registro A: 4 bytes de IPv4
            if (pos + 4 > buf_len) return false;
            out_ip->octets[0] = buf[pos];
            out_ip->octets[1] = buf[pos + 1];
            out_ip->octets[2] = buf[pos + 2];
            out_ip->octets[3] = buf[pos + 3];
            return true;
        }

        pos += rdlen;
        if (pos > buf_len) return false;
    }

    return false; // Nenhum registro A encontrado
}

// ============================================================
// dns_resolve — resolve hostname para IP
// ============================================================
bool dns_resolve(const char *hostname, ip_addr_t *out_ip) {
    if (!hostname || !out_ip) return false;

    // Se já é um IP, converte diretamente
    if (str_to_ip(hostname, out_ip)) return true;

    // Busca no cache
    if (dns_cache_lookup(hostname, out_ip)) return true;

    // Prepara query
    uint16_t qid = dns_query_id++;

    // Envia query DNS (com retry)
    for (int attempt = 0; attempt < 2; attempt++) {
        if (!dns_send_query(hostname, qid)) continue;

        // Espera resposta via UDP síncrono
        uint8_t resp_buf[512];
        uint16_t resp_len = 0;
        ip_addr_t resp_ip;
        uint16_t resp_port = 0;

        // A porta local usada foi 5353 + (qid & 0xFF)
        uint16_t local_port = 5353 + (qid & 0xFF);

        bool got = udp_recv_sync(local_port, resp_buf, sizeof(resp_buf),
                                 &resp_len, &resp_ip, &resp_port,
                                 DNS_TIMEOUT_MS);

        if (got && resp_len > 0) {
            if (dns_parse_response(resp_buf, resp_len, qid, out_ip)) {
                dns_cache_store(hostname, *out_ip);
                stats.responses_ok++;
                return true;
            }
        }

        // Retry com novo ID
        qid = dns_query_id++;
    }

    stats.responses_fail++;
    return false;
}

// ============================================================
// dns_init
// ============================================================
void dns_init(void) {
    kmemset(&stats, 0, sizeof(stats));
    kmemset(cache, 0, sizeof(cache));
    dns_query_id = 1;

    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("DNS: resolver pronto (10.0.2.3)\n", THEME_BOOT);
}
