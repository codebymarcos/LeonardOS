// LeonardOS - Socket API
// Interface unificada sobre TCP e UDP
//
// Uso:
//   int fd = socket(SOCK_TCP);
//   socket_connect(fd, ip, port);
//   socket_send(fd, data, len);
//   int n = socket_recv(fd, buf, sizeof(buf), timeout_ms);
//   socket_close(fd);

#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "../common/types.h"
#include "net_config.h"

// ============================================================
// Constantes
// ============================================================
#define SOCK_TCP        1       // Socket TCP (stream)
#define SOCK_UDP        2       // Socket UDP (datagram)
#define SOCKET_MAX      8       // Máximo de sockets simultâneos
#define SOCKET_ERROR    (-1)    // Erro genérico

// ============================================================
// API pública — 5 funções principais
// ============================================================

// Cria um socket do tipo especificado (SOCK_TCP ou SOCK_UDP)
// Retorna file descriptor (0..7) ou SOCKET_ERROR
int socket(int type);

// Conecta socket a um endereço remoto (TCP: 3-way handshake, UDP: associa)
// timeout_ms: timeout para conexão TCP (ignorado para UDP)
// Retorna 0 se ok, SOCKET_ERROR se falhou
int socket_connect(int fd, ip_addr_t dst_ip, uint16_t dst_port, uint32_t timeout_ms);

// Envia dados por um socket conectado
// Retorna bytes enviados ou SOCKET_ERROR
int socket_send(int fd, const void *data, uint16_t len);

// Recebe dados de um socket conectado
// Retorna bytes lidos, 0 se timeout/EOF, SOCKET_ERROR se erro
int socket_recv(int fd, void *buf, uint16_t buf_size, uint32_t timeout_ms);

// Fecha um socket e libera recursos
void socket_close(int fd);

// ============================================================
// Funções auxiliares
// ============================================================

// Verifica se socket está conectado
bool socket_is_connected(int fd);

// Retorna bytes disponíveis para leitura sem bloquear
uint16_t socket_available(int fd);

// Verifica se o peer fechou a conexão
bool socket_peer_closed(int fd);

// Inicializa o módulo de sockets
void socket_init(void);

#endif
