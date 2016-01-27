#define INFO_PRIKAZ
#define INFO_LOG
#define BRISANJE_ZASTARJELIH_LOG
#define ERROR_LOG
#define INTERAKCIJA

#include "PopisTipova.h"

#include <iostream>
#include <vector>
#include <list>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <queue>

#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>

/* standardni u/i tokovi */
using std::cout;
using std::endl;
using std::cin;

/* broj sekundi izmedju dvije provjere zastarjelih zapisa u bazi */
#define VRIJEME_ZA_PROVJERE_ZAPISA_U_BAZI_SEC 60
#define VRIJEME_ZASTARJELOG_ZAPISA 100

/******************************************************************************/
/** UCITAVANJE KONFIGURACIJSKE DATOTEKE                                      **/
/******************************************************************************/

/* varijable koje se dobavljaju iz konfiguracijske datoteke */
std::string SERV_IP_ADDRESS;
int SERV_PORT;
int MAX_BUFFER_SIZE;
#define MAX_BUFFER_LOCAL_SIZE 10000
int MAX_N_THREAD;

/* minimalno TTL u sekundama (kod zapisa u 'bazu') */
#define MIN_TTL_SEC 20

/* ucitavanje konfiguracijskih podataka (parametara iz datoteke) */
void ucitaj_konfig_podatke() {
    
    /*    Format konfiguracijske datoteke:
     *    
     *    ip_address port [description]
     *    max_buffer_size(broj zapisan u ASCII formatu) [description]
     *    max_size_of_thread_pool(broj zapisan u ASCII formatu) [description]
     *    
     *    [ ] - opcionalno
     */
    
    std::string info;
    std::ifstream idat("parametri.config");
    
#ifdef INFO_LOG
    cout << "Ucitavanje parametara iz konfiguracijske datoteke..." << endl;
#endif
    
    /* provjera da li je datoteka dostupna, ako nije - prekid programa */
    if(idat) {
        /* ucitavanje podataka */
        idat >> SERV_IP_ADDRESS >> SERV_PORT;
        getline(idat, info);
        
        idat >> MAX_BUFFER_SIZE;
        getline(idat, info);
        
        idat >> MAX_N_THREAD;
        getline(idat, info);
        
        /* prikaz procitanih podataka */
#ifdef INFO_LOG
        cout << " IP: " << SERV_IP_ADDRESS << ":" << SERV_PORT  << endl;
        cout << " Velicina buffer-a: " << MAX_BUFFER_SIZE << endl;
        cout << " Broj dretvi: " << MAX_N_THREAD << endl;
#endif
        
        /* zatvaranje datoteke */
        idat.close();
        
#ifdef INFO_LOG
        cout << "Parametri procitani." << endl;
#endif
    } else {
        /* nema datoteke - ispis obavijesti, a kasnije i prekid programa */
#ifdef ERROR_LOG
        cout << "Greska kod ucitavanja parametara." << endl;
#endif
        
        /* prekini program - nema konfiguracijskih parametara */
        exit(-1);
    }
}

/******************************************************************************/
/** STRUKTURE PORUKA                                                         **/
/******************************************************************************/

/* struktura poruke */
struct sadresa{
    uint8_t tip_adrese;
    union {
        uint32_t adr_ipv4;
        struct {
            uint64_t adr_ipv6_1;
            uint64_t adr_ipv6_2;
        };
        unsigned char adr_hostname[100];
    };
    uint16_t broj_porta;
};

struct smsg_multimedia {
    uint64_t identif_strujanja;
    uint8_t rbr_paketa;
    uint16_t vremenska_oznaka;
    unsigned char parametri_strujanja[250];
    unsigned char podaci[250];
};
struct smsg_stream_advertisement {
    uint64_t identif_strujanja;
    sadresa adresa;
};
struct smsg_stream_registered {
    uint64_t identif_strujanja;
    uint16_t TTL_u_sec;
    sadresa adresa; /* bez tipa ADR_HOSTNAME */
};
struct smsg_identifier_not_usable {
    uint64_t identif_strujanja;
    sadresa adresa; /* bez tipa ADR_HOSTNAME */
};
struct smsg_stream_remove {
    uint64_t identif_strujanja;
};
struct smsg_request_streaming {
    uint64_t identif_strujanja;
    unsigned char parametri_multimedije[250];
};
struct smsg_find_stream_source {
    uint64_t identif_strujanja;
};
struct smsg_stream_source_data {
    uint64_t identif_strujanja;
    sadresa javna_adresa_reproduktora;
    sadresa javna_adresa_izvora;
    sadresa lokalna_adresa_izvora;
};
struct smsg_forward_player_ready {
    uint64_t identif_strujanja;
    sadresa javna_adresa;
    sadresa lokalna_adresa;
};
struct smsg_player_ready {
    uint64_t identif_strujanja;
    sadresa javna_adresa;
    sadresa lokalna_adresa;
};
struct smsg_source_ready {
    uint64_t identif_strujanja;
};
struct smsg_req_relay_list {
};
struct smsg_relay_list {
    uint32_t broj_zapisa;
    sadresa* posluzitelji;
};
struct smsg_shutting_down {
    uint64_t identif_strujanja;
};
struct smsg_please_forward {
    sadresa adresa;
    unsigned char poruka[250];
};
struct smsg_ping {
    unsigned char podaci[250];
};
struct smsg_pong {
    unsigned char podaci[250];
};
struct smsg_pong_reg_req {
    unsigned char podaci[250];
};
struct smsg_register_forwarding {
    sadresa adresa;
};

struct sporuka {
    uint8_t tip_poruke; /* tip poruke odredjuje koji zapis koristiti iz unije */
    union {
        smsg_multimedia             msg_multimedia;
        smsg_stream_advertisement   msg_stream_advertisement;
        smsg_stream_registered      msg_stream_registered;
        smsg_identifier_not_usable  msg_identifier_not_usable;
        smsg_stream_remove          msg_stream_remove;
        smsg_request_streaming      msg_request_streaming;
        smsg_find_stream_source     msg_find_stream_source;
        smsg_stream_source_data     msg_stream_source_data;
        smsg_forward_player_ready   msg_forward_player_ready;
        smsg_player_ready           msg_player_ready;
        smsg_source_ready           msg_source_ready;
        smsg_req_relay_list         msg_req_relay_list;
        smsg_relay_list             msg_relay_list;
        smsg_shutting_down          msg_shutting_down;
        smsg_please_forward         msg_please_forward;
        smsg_ping                   msg_ping;
        smsg_pong                   msg_pong;
        smsg_pong_reg_req           msg_pong_reg_req;
        smsg_register_forwarding    msg_register_forwarding;
    };
};

sporuka pretvori_poruku_u_strukturu(unsigned char* poruka, int n) {
    sporuka p;
    int m = 0;
    p.tip_poruke = poruka[m];
    switch(p.tip_poruke) {
        case MSG_PING: {
            strcpy((char*)p.msg_ping.podaci, (char*)(poruka+1) );
            
            break;
        }
        
        case MSG_PONG: {
            strcpy((char*)p.msg_pong.podaci, (char*)(poruka+1) );
            
            break;
        }
        
        case MSG_PONG_REG_REQ: {
            strcpy((char*)p.msg_pong_reg_req.podaci, (char*)(poruka+1) );
            
            break;
        }
        
        case MSG_STREAM_ADVERTISEMENT: {
            p.msg_stream_advertisement.identif_strujanja = 
                poruka[m+1]*0x100000000000000 + 
                poruka[m+2]*0x1000000000000 + 
                poruka[m+3]*0x10000000000 + 
                poruka[m+4]*0x100000000 + 
                poruka[m+5]*0x1000000 + 
                poruka[m+6]*0x10000 + 
                poruka[m+7]*0x100 + 
                poruka[m+8];
            m += 9;
            
            p.msg_stream_advertisement.adresa.tip_adrese = poruka[m];
            switch((uint8_t)poruka[m]) {
                case ADR_IPv4:
                    p.msg_stream_advertisement.adresa.adr_ipv4 = 
                        ntohl(
                        poruka[m+1]*0x1000000 + 
                        poruka[m+2]*0x10000 + 
                        poruka[m+3]*0x100 + 
                        poruka[m+4]);
                    break;
                case ADR_IPv6:
                    p.msg_stream_advertisement.adresa.adr_ipv6_1 = 
                        be64toh(
                        poruka[m+1]*0x100000000000000 + 
                        poruka[m+2]*0x1000000000000 + 
                        poruka[m+3]*0x10000000000 + 
                        poruka[m+4]*0x100000000 + 
                        poruka[m+5]*0x1000000 + 
                        poruka[m+6]*0x10000 + 
                        poruka[m+7]*0x100 + 
                        poruka[m+8]);
                    m += 8;
                    p.msg_stream_advertisement.adresa.adr_ipv6_2 = 
                        be64toh(
                        poruka[m+1]*0x100000000000000 + 
                        poruka[m+2]*0x1000000000000 + 
                        poruka[m+3]*0x10000000000 + 
                        poruka[m+4]*0x100000000 + 
                        poruka[m+5]*0x1000000 + 
                        poruka[m+6]*0x10000 + 
                        poruka[m+7]*0x100 + 
                        poruka[m+8]);
                    m += 9;
                    break;
            }
            p.msg_stream_advertisement.adresa.broj_porta = 
                ntohs(poruka[m]*0x100 + poruka[m+1]);
            
            break;
        }
        
        case MSG_STREAM_REGISTERED: {
            p.msg_stream_registered.identif_strujanja = 
                be64toh(
                poruka[m+1]*0x100000000000000 + 
                poruka[m+2]*0x1000000000000 + 
                poruka[m+3]*0x10000000000 + 
                poruka[m+4]*0x100000000 + 
                poruka[m+5]*0x1000000 + 
                poruka[m+6]*0x10000 + 
                poruka[m+7]*0x100 + 
                poruka[m+8]);
            m += 9;
            
            p.msg_stream_registered.TTL_u_sec = ntohs(poruka[m]*0x100 + poruka[m+1]);
            m += 2;
            
            p.msg_stream_registered.adresa.tip_adrese = poruka[m];
            switch((uint8_t)poruka[m]) {
                case ADR_IPv4:
                    p.msg_stream_registered.adresa.adr_ipv4 = 
                        ntohl(
                        poruka[m+1]*0x1000000 + 
                        poruka[m+2]*0x10000 + 
                        poruka[m+3]*0x100 + 
                        poruka[m+4]);
                    break;
                case ADR_IPv6:
                    p.msg_stream_registered.adresa.adr_ipv6_1 = 
                        be64toh(
                        poruka[m+1]*0x100000000000000 + 
                        poruka[m+2]*0x1000000000000 + 
                        poruka[m+3]*0x10000000000 + 
                        poruka[m+4]*0x100000000 + 
                        poruka[m+5]*0x1000000 + 
                        poruka[m+6]*0x10000 + 
                        poruka[m+7]*0x100 + 
                        poruka[m+8]);
                    m += 8;
                    p.msg_stream_registered.adresa.adr_ipv6_2 = 
                        be64toh(
                        poruka[m+1]*0x100000000000000 + 
                        poruka[m+2]*0x1000000000000 + 
                        poruka[m+3]*0x10000000000 + 
                        poruka[m+4]*0x100000000 + 
                        poruka[m+5]*0x1000000 + 
                        poruka[m+6]*0x10000 + 
                        poruka[m+7]*0x100 + 
                        poruka[m+8]);
                    m += 9;
                    break;
            }
            p.msg_stream_registered.adresa.broj_porta = 
                ntohs(poruka[m]*0x100 + poruka[m+1]);
            
            break;
        }
        
        case MSG_IDENTIFIER_NOT_USABLE: {
            p.msg_identifier_not_usable.identif_strujanja = 
                be64toh(
                poruka[m+1]*0x100000000000000 + 
                poruka[m+2]*0x1000000000000 + 
                poruka[m+3]*0x10000000000 + 
                poruka[m+4]*0x100000000 + 
                poruka[m+5]*0x1000000 + 
                poruka[m+6]*0x10000 + 
                poruka[m+7]*0x100 + 
                poruka[m+8]);
            m += 9;
            
            p.msg_identifier_not_usable.adresa.tip_adrese = poruka[m];
            switch((uint8_t)poruka[m]) {
                case ADR_IPv4:
                    p.msg_identifier_not_usable.adresa.adr_ipv4 = 
                        ntohl(
                        poruka[m+1]*0x1000000 + 
                        poruka[m+2]*0x10000 + 
                        poruka[m+3]*0x100 + 
                        poruka[m+4]);
                    break;
                case ADR_IPv6:
                    p.msg_identifier_not_usable.adresa.adr_ipv6_1 = 
                        be64toh(
                        poruka[m+1]*0x100000000000000 + 
                        poruka[m+2]*0x1000000000000 + 
                        poruka[m+3]*0x10000000000 + 
                        poruka[m+4]*0x100000000 + 
                        poruka[m+5]*0x1000000 + 
                        poruka[m+6]*0x10000 + 
                        poruka[m+7]*0x100 + 
                        poruka[m+8]);
                    m += 8;
                    p.msg_identifier_not_usable.adresa.adr_ipv6_2 = 
                        be64toh(
                        poruka[m+1]*0x100000000000000 + 
                        poruka[m+2]*0x1000000000000 + 
                        poruka[m+3]*0x10000000000 + 
                        poruka[m+4]*0x100000000 + 
                        poruka[m+5]*0x1000000 + 
                        poruka[m+6]*0x10000 + 
                        poruka[m+7]*0x100 + 
                        poruka[m+8]);
                    m += 9;
                    break;
            }
            p.msg_identifier_not_usable.adresa.broj_porta = 
                ntohs(poruka[m]*0x100 + poruka[m+1]);
            
            break;
        }
        
        case MSG_FIND_STREAM_SOURCE: {
            p.msg_find_stream_source.identif_strujanja = 
                be64toh(
                poruka[m+1]*0x100000000000000 + 
                poruka[m+2]*0x1000000000000 + 
                poruka[m+3]*0x10000000000 + 
                poruka[m+4]*0x100000000 + 
                poruka[m+5]*0x1000000 + 
                poruka[m+6]*0x10000 + 
                poruka[m+7]*0x100 + 
                poruka[m+8]);
            
            break;
        }
        
        case MSG_STREAM_SOURCE_DATA: {
            p.msg_stream_source_data.identif_strujanja = 
                be64toh(
                poruka[m+1]*0x100000000000000 + 
                poruka[m+2]*0x1000000000000 + 
                poruka[m+3]*0x10000000000 + 
                poruka[m+4]*0x100000000 + 
                poruka[m+5]*0x1000000 + 
                poruka[m+6]*0x10000 + 
                poruka[m+7]*0x100 + 
                poruka[m+8]);
            m += 9;
            
            p.msg_stream_source_data.javna_adresa_reproduktora.tip_adrese = poruka[m];
            switch((uint8_t)poruka[m]) {
                case ADR_IPv4:
                 p.msg_stream_source_data.javna_adresa_reproduktora.adr_ipv4 = 
                        ntohl(
                        poruka[m+1]*0x1000000 + 
                        poruka[m+2]*0x10000 + 
                        poruka[m+3]*0x100 + 
                        poruka[m+4]);
                    m += 5;
                    break;
                case ADR_IPv6:
                 p.msg_stream_source_data.javna_adresa_reproduktora.adr_ipv6_1 = 
                        be64toh(
                        poruka[m+1]*0x100000000000000 + 
                        poruka[m+2]*0x1000000000000 + 
                        poruka[m+3]*0x10000000000 + 
                        poruka[m+4]*0x100000000 + 
                        poruka[m+5]*0x1000000 + 
                        poruka[m+6]*0x10000 + 
                        poruka[m+7]*0x100 + 
                        poruka[m+8]);
                    m += 8;
                 p.msg_stream_source_data.javna_adresa_reproduktora.adr_ipv6_2 = 
                        be64toh(
                        poruka[m+1]*0x100000000000000 + 
                        poruka[m+2]*0x1000000000000 + 
                        poruka[m+3]*0x10000000000 + 
                        poruka[m+4]*0x100000000 + 
                        poruka[m+5]*0x1000000 + 
                        poruka[m+6]*0x10000 + 
                        poruka[m+7]*0x100 + 
                        poruka[m+8]);
                    m += 9;
                    break;
            }
            p.msg_stream_source_data.javna_adresa_reproduktora.broj_porta = 
                ntohs(poruka[m]*0x100 + poruka[m+1]);
            m += 2;
            
            p.msg_stream_source_data.javna_adresa_izvora.tip_adrese = poruka[m];
            switch((uint8_t)poruka[m]) {
                case ADR_IPv4:
                    p.msg_stream_source_data.javna_adresa_izvora.adr_ipv4 = 
                        ntohl(
                        poruka[m+1]*0x1000000 + 
                        poruka[m+2]*0x10000 + 
                        poruka[m+3]*0x100 + 
                        poruka[m+4]);
                    m += 5;
                    break;
                case ADR_IPv6:
                    p.msg_stream_source_data.javna_adresa_izvora.adr_ipv6_1 = 
                        be64toh(
                        poruka[m+1]*0x100000000000000 + 
                        poruka[m+2]*0x1000000000000 + 
                        poruka[m+3]*0x10000000000 + 
                        poruka[m+4]*0x100000000 + 
                        poruka[m+5]*0x1000000 + 
                        poruka[m+6]*0x10000 + 
                        poruka[m+7]*0x100 + 
                        poruka[m+8]);
                    m += 8;
                    p.msg_stream_source_data.javna_adresa_izvora.adr_ipv6_2 = 
                        be64toh(
                        poruka[m+1]*0x100000000000000 + 
                        poruka[m+2]*0x1000000000000 + 
                        poruka[m+3]*0x10000000000 + 
                        poruka[m+4]*0x100000000 + 
                        poruka[m+5]*0x1000000 + 
                        poruka[m+6]*0x10000 + 
                        poruka[m+7]*0x100 + 
                        poruka[m+8]);
                    m += 9;
                    break;
            }
            p.msg_stream_source_data.javna_adresa_izvora.broj_porta = 
                ntohs(poruka[m]*0x100 + poruka[m+1]);
            m += 2;
            
            p.msg_stream_source_data.lokalna_adresa_izvora.tip_adrese = 
                poruka[m];
            switch((uint8_t)poruka[m]) {
                case ADR_IPv4:
                    p.msg_stream_source_data.lokalna_adresa_izvora.adr_ipv4 = 
                        ntohl(
                        poruka[m+1]*0x1000000 + 
                        poruka[m+2]*0x10000 + 
                        poruka[m+3]*0x100 + 
                        poruka[m+4]);
                    m += 5;
                    break;
                case ADR_IPv6:
                    p.msg_stream_source_data.lokalna_adresa_izvora.adr_ipv6_1 = 
                        be64toh(
                        poruka[m+1]*0x100000000000000 + 
                        poruka[m+2]*0x1000000000000 + 
                        poruka[m+3]*0x10000000000 + 
                        poruka[m+4]*0x100000000 + 
                        poruka[m+5]*0x1000000 + 
                        poruka[m+6]*0x10000 + 
                        poruka[m+7]*0x100 + 
                        poruka[m+8]);
                    m += 8;
                    p.msg_stream_source_data.lokalna_adresa_izvora.adr_ipv6_2 = 
                        be64toh(
                        poruka[m+1]*0x100000000000000 + 
                        poruka[m+2]*0x1000000000000 + 
                        poruka[m+3]*0x10000000000 + 
                        poruka[m+4]*0x100000000 + 
                        poruka[m+5]*0x1000000 + 
                        poruka[m+6]*0x10000 + 
                        poruka[m+7]*0x100 + 
                        poruka[m+8]);
                    m += 9;
                    break;
            }
            p.msg_stream_source_data.lokalna_adresa_izvora.broj_porta = 
                ntohs(poruka[m]*0x100 + poruka[m+1]);
            
            break;
        }
        
        case MSG_STREAM_REMOVE: {
            p.msg_stream_remove.identif_strujanja = 
                be64toh(
                poruka[m+1]*0x100000000000000 + 
                poruka[m+2]*0x1000000000000 + 
                poruka[m+3]*0x10000000000 + 
                poruka[m+4]*0x100000000 + 
                poruka[m+5]*0x1000000 + 
                poruka[m+6]*0x10000 + 
                poruka[m+7]*0x100 + 
                poruka[m+8]);
            
            break;
        }
        
        case MSG_MULTIMEDIA: break;
        case MSG_REQUEST_STREAMING: break;
        case MSG_FORWARD_PLAYER_READY: break;
        case MSG_PLAYER_READY: break;
        case MSG_SOURCE_READY: break;
        case MSG_REQ_RELAY_LIST: break;
        case MSG_RELAY_LIST: break;
        case MSG_SHUTTING_DOWN: break;
        case MSG_PLEASE_FORWARD: break;
        case MSG_REGISTER_FORWARDING: break;
        
        default: break;
    }
    
    return p;
}

std::string pretvori_poruku_iz_strukture(sporuka poruka) {
    std::string p;
    switch(poruka.tip_poruke) {
        case MSG_PING: {
            p += MSG_PING;
            p += (char*)poruka.msg_ping.podaci;
            
            break;
        }
        
        case MSG_PONG: {
            p += MSG_PONG;
            p += (char*)poruka.msg_pong.podaci;
            
            break;
        }
        
        case MSG_PONG_REG_REQ: {
            p += MSG_PONG_REG_REQ;
            p += (char*)poruka.msg_pong_reg_req.podaci;
            
            break;
        }
        
        case MSG_STREAM_ADVERTISEMENT: {
            uint64_t identif_strujanja = 
                htobe64(poruka.msg_stream_advertisement.identif_strujanja);
            sadresa adresa = poruka.msg_stream_advertisement.adresa;
            adresa.broj_porta = htons(adresa.broj_porta);
            
            
            char id_str[9];
            id_str[8] = '\0';
            for(int i=7; i>=0; i--) {
                id_str[i] = identif_strujanja%0x100;
                identif_strujanja /= 0x100;
            }
            
            p += MSG_STREAM_ADVERTISEMENT;
            p += id_str;
            p += (char)adresa.tip_adrese;
            switch(adresa.tip_adrese) {
                case ADR_IPv4: {
                    char adr[5];
                    adr[4] = '\0';
                    uint32_t ipv4 = htonl(adresa.adr_ipv4);
                    for(int i=3; i>=0; i--) {
                        adr[i] = ipv4%0x100;
                        ipv4 /= 0x100;
                    }
                    p += adr;
                    
                    break;
                }
                
                case ADR_IPv6: {
                    char adr[17];
                    adr[16] = '\0';
                    uint64_t ipv6_1 = adresa.adr_ipv6_1;
                    uint64_t ipv6_2 = adresa.adr_ipv6_2;
                    for(int i=7; i>=0; i--) {
                        adr[i] = ipv6_1%0x100;
                        ipv6_1 /= 0x100;
                    }
                    for(int i=7; i>=0; i--) {
                        adr[i+8] = ipv6_2%0x100;
                        ipv6_2 /= 0x100;
                    }
                    p += adr;
                    
                    break;
                }
                
                case ADR_HOSTNAME: {
                    p += (char*)adresa.adr_hostname;
                    
                    break;
                }
                
                default: break;
            }
            p += adresa.broj_porta/0x100;
            p += adresa.broj_porta%0x100;
            
            break;
        }
        
        case MSG_STREAM_REGISTERED: {
            uint64_t identif_strujanja = 
                htobe64(poruka.msg_stream_registered.identif_strujanja);
            sadresa adresa = poruka.msg_stream_registered.adresa;
            adresa.broj_porta = htons(adresa.broj_porta);
            
            unsigned char id_str[9];
            id_str[8] = '\0';
            for(int i=7; i>=0; i--) {
                id_str[i] = identif_strujanja%0x100;
                identif_strujanja /= 0x100;
            }
            
            unsigned char ttl0[3];
            ttl0[2] = '\0';
            poruka.msg_stream_registered.TTL_u_sec =
                htons(poruka.msg_stream_registered.TTL_u_sec);
            ttl0[0] = poruka.msg_stream_registered.TTL_u_sec/0x100;
            ttl0[1] = poruka.msg_stream_registered.TTL_u_sec%0x100;
            
            p += MSG_STREAM_REGISTERED;
            p += (char*)id_str;
            p += (char*)ttl0;
            p += (char)adresa.tip_adrese;
            switch(adresa.tip_adrese) {
                case ADR_IPv4: {
                    char adr[5];
                    adr[4] = '\0';
                    uint32_t ipv4 = htonl(adresa.adr_ipv4);
                    for(int i=3; i>=0; i--) {
                        adr[i] = ipv4%0x100;
                        ipv4 /= 0x100;
                    }
                    p += adr;
                    
                    break;
                }
                
                case ADR_IPv6: {
                    char adr[17];
                    adr[16] = '\0';
                    uint64_t ipv6_1 = adresa.adr_ipv6_1;
                    uint64_t ipv6_2 = adresa.adr_ipv6_2;
                    for(int i=7; i>=0; i--) {
                        adr[i] = ipv6_1%0x100;
                        ipv6_1 /= 0x100;
                    }
                    for(int i=7; i>=0; i--) {
                        adr[i+8] = ipv6_2%0x100;
                        ipv6_2 /= 0x100;
                    }
                    p += adr;
                    
                    break;
                }
                
                case ADR_HOSTNAME: {
                    p += (char*)adresa.adr_hostname;
                    
                    break;
                }
                
                default: break;
            }
            p += adresa.broj_porta/0x100;
            p += adresa.broj_porta%0x100;
            
            break;
        }
        
        case MSG_IDENTIFIER_NOT_USABLE: {
            uint64_t identif_strujanja = 
                htobe64(poruka.msg_identifier_not_usable.identif_strujanja);
            sadresa adresa = poruka.msg_identifier_not_usable.adresa;
            adresa.broj_porta = htons(adresa.broj_porta);
            
            unsigned char id_str[9];
            id_str[8] = '\0';
            for(int i=7; i>=0; i--) {
                id_str[i] = identif_strujanja%0x100;
                identif_strujanja /= 0x100;
            }
            
            p += MSG_IDENTIFIER_NOT_USABLE;
            p += (char*)id_str;
            p += (char)adresa.tip_adrese;
            switch(adresa.tip_adrese) {
                case ADR_IPv4: {
                    char adr[5];
                    adr[4] = '\0';
                    uint32_t ipv4 = htonl(adresa.adr_ipv4);
                    for(int i=3; i>=0; i--) {
                        adr[i] = ipv4%0x100;
                        ipv4 /= 0x100;
                    }
                    p += adr;
                    
                    break;
                }
                
                case ADR_IPv6: {
                    char adr[17];
                    adr[16] = '\0';
                    uint64_t ipv6_1 = adresa.adr_ipv6_1;
                    uint64_t ipv6_2 = adresa.adr_ipv6_2;
                    for(int i=7; i>=0; i--) {
                        adr[i] = ipv6_1%0x100;
                        ipv6_1 /= 0x100;
                    }
                    for(int i=7; i>=0; i--) {
                        adr[i+8] = ipv6_2%0x100;
                        ipv6_2 /= 0x100;
                    }
                    p += adr;
                    
                    break;
                }
                
                case ADR_HOSTNAME: {
                    p += (char*)adresa.adr_hostname;
                    
                    break;
                }
                
                default: break;
            }
            p += adresa.broj_porta/0x100;
            p += adresa.broj_porta%0x100;
            
            break;
        }
        
        case MSG_FIND_STREAM_SOURCE: break;
        case MSG_STREAM_SOURCE_DATA: break;
        
        case MSG_STREAM_REMOVE: {
            uint64_t identif_strujanja = 
                htobe64(poruka.msg_stream_remove.identif_strujanja);
            
            unsigned char id_str[9];
            id_str[8] = '\0';
            for(int i=7; i>=0; i--) {
                id_str[i] = identif_strujanja%0x100;
                identif_strujanja /= 0x100;
            }
            
            p += MSG_STREAM_REMOVE;
            p += (char*)id_str;
            
            break;
        }
        
        case MSG_MULTIMEDIA: break;
        case MSG_REQUEST_STREAMING: break;
        case MSG_FORWARD_PLAYER_READY: break;
        case MSG_PLAYER_READY: break;
        case MSG_SOURCE_READY: break;
        case MSG_REQ_RELAY_LIST: break;
        case MSG_RELAY_LIST: {
            p += MSG_RELAY_LIST;
            
            uint32_t br_zap = htonl(poruka.msg_relay_list.broj_zapisa);
            unsigned char br_zap_str[4];
            br_zap_str[3] = '\0';
            for(int i=3; i>=0; i--) {
                br_zap_str[i] = br_zap%0x100;
                br_zap /= 0x100;
            }
            
            p += br_zap_str[0] + br_zap_str[1] + br_zap_str[2] + br_zap_str[3];
            
            for(int i=0; i<poruka.msg_relay_list.broj_zapisa; i++) {
                sadresa adresa = poruka.msg_relay_list.posluzitelji[i];
                adresa.broj_porta = htons(adresa.broj_porta);
                p += (char)adresa.tip_adrese;
                switch(adresa.tip_adrese) {
                    case ADR_IPv4: {
                        char adr[5];
                        adr[4] = '\0';
                        uint32_t ipv4 = htonl(adresa.adr_ipv4);
                        for(int i=3; i>=0; i--) {
                            adr[i] = ipv4%0x100;
                            ipv4 /= 0x100;
                        }
                        p += adr;
                        
                        break;
                    }
                    
                    case ADR_IPv6: {
                        char adr[17];
                        adr[16] = '\0';
                        uint64_t ipv6_1 = adresa.adr_ipv6_1;
                        uint64_t ipv6_2 = adresa.adr_ipv6_2;
                        for(int i=7; i>=0; i--) {
                            adr[i] = ipv6_1%0x100;
                            ipv6_1 /= 0x100;
                        }
                        for(int i=7; i>=0; i--) {
                            adr[i+8] = ipv6_2%0x100;
                            ipv6_2 /= 0x100;
                        }
                        p += adr;
                        
                        break;
                    }
                    
                    case ADR_HOSTNAME: {
                        p += (char*)adresa.adr_hostname;
                        
                        break;
                    }
                    
                    default: break;
                }
                p += adresa.broj_porta/0x100;
                p += adresa.broj_porta%0x100;
            }
            
            break;
        }
        case MSG_SHUTTING_DOWN: break;
        case MSG_PLEASE_FORWARD: break;
        case MSG_REGISTER_FORWARDING: break;
        
        default: break;
    }
    
    /* vraca niz bajtova poruke */
    return p;
}

/******************************************************************************/
/** POPIS RELAYA                                                             **/
/******************************************************************************/

/* popis relay-a */
std::vector<sadresa> popis_relaya;

/* ucitavanje popisa relay-a iz datoteke */
void ucitaj_relaye() {
    
    /*    Format datoteke s popisom relay-a:
     *    
     *    [h/ip4/ip6] ip/hostname port
     *    (...)
     *    
     */
    
    sadresa pom;
    std::string tip, adresa;
    int br_porta;
    
    /* otvaranje datoteke s popisom relay-a */
    std::ifstream idat("popis_relaya.txt");
    
    /* provjera da li je datoteka dostupna */
    if(idat) {
#ifdef INFO_LOG
        cout << "Ucitavanje popisa relay-a..." << endl;
#endif
        while(1) {
            /* svaki prolaz petljom ucitava podatke jednog relay-a */
            idat >> tip >> adresa >> br_porta; //inet_pton / inet_ntop
            if(idat.eof() )
                break;
            
            in_addr adr;
            
            if(tip=="h") {
                pom.tip_adrese = ADR_HOSTNAME;
                strcpy((char*)pom.adr_hostname, (char*)adresa.c_str() );
            
            } else if(tip=="ip4") {
                pom.tip_adrese = ADR_IPv4;
                
                if(!inet_aton(adresa.c_str(), (in_addr*)&adr) ) {
#ifdef INFO_LOG
                    cout << "adresa nije ispravnog zapisa" << endl;
#endif
                }
                pom.adr_ipv4 = adr.s_addr;
            } else if(tip=="ip6") {
                pom.tip_adrese = ADR_IPv6;
                
            } else {
                cout << "neispravan zapis adrese relay-a" << endl;
                continue;
            }
            
            pom.broj_porta = br_porta;
            
            /* dodaj novi relay na popis */
            popis_relaya.push_back(pom);
        }
        
        /* zatvaranje datoteke */
        idat.close();
#ifdef INFO_LOG
        cout << "Popis relay-a procitan." << endl;
#endif
    } else {
#ifdef ERROR_LOG
        cout << "Popis relay-a ne postoji." << endl;
#endif
    }
}

/* odgovor tipa MSG_RELAY_LIST na poruku MSG_REQ_RELAY_LIST */
std::string poruka_popis_relaya_def;

/* provjeri dostupnost za sve relaye sa popisa */
void provjeri_dostupnost_relaya() {
    in_addr p;
    /* prolaz kroz sve relay-e te provjera da li je relay dostupan */
    for(sadresa& r : popis_relaya) {
#ifdef INFO_LOG
        cout << "Provjeravam relay..." << endl;
        switch(r.tip_adrese) {
            case ADR_HOSTNAME:
                cout << r.adr_hostname << ":" << r.broj_porta << endl;
                break;
            
            case ADR_IPv4: {
                p.s_addr = r.adr_ipv4;
                cout << inet_ntoa(p) << ":" << r.broj_porta << endl;
                break;
            }
            
            case ADR_IPv6:
                break;
            
            default: break;
        }
#endif
        
        /*** ... ***/
        
#ifdef INFO_LOG
        cout << "relay je dostupan." << endl;
#endif
    }
    
    sporuka odgovor;
    odgovor.tip_poruke = MSG_RELAY_LIST;
    odgovor.msg_relay_list.broj_zapisa = popis_relaya.size();
    odgovor.msg_relay_list.posluzitelji = new sadresa[odgovor.msg_relay_list.broj_zapisa];
    for(int i=0; i<odgovor.msg_relay_list.broj_zapisa; i++)
        odgovor.msg_relay_list.posluzitelji[i] = popis_relaya[i];
    
    poruka_popis_relaya_def = pretvori_poruku_iz_strukture(odgovor);
}

/******************************************************************************/
/** RAD S BAZOM                                                              **/
/******************************************************************************/

/* struktura zapisa u privremenu bazu */
struct szapis {
    sadresa adresa;
    time_t vrijeme;
    sporuka poruka;
};
struct svremenski_zapis {
    uint64_t identifikator;
    time_t vrijeme;
};

/* privremena 'baza' */
std::unordered_map<uint64_t, szapis> baza;
std::unordered_map<uint64_t, szapis>::iterator baza_it;

std::vector<svremenski_zapis> popis_dodavanja_zapisa;
std::vector<svremenski_zapis>::iterator pdz_it;

/* pomocne varijable (monitor/sock_id/adrese) */
std::mutex monitor;
std::mutex m_baza;
int sock_id;
sockaddr_in server_addr;

/* dretva za brisanje zastarjelih zapisa */
void brisi_zastarjeli() {
    /* podatak do kojeg se vremena brisu svi zapisi iz baze */
    time_t trenutno_vrijeme;
    int i, k, opet;
    
    /* ponavljanje brisanja u periodama */
    while(1) {
        sleep(VRIJEME_ZA_PROVJERE_ZAPISA_U_BAZI_SEC);
        
        /* zakljucavanje pristupa bazi */
        m_baza.lock();
        
        /* dobavljanje trenutnog vremena */
        trenutno_vrijeme = time(0) - VRIJEME_ZASTARJELOG_ZAPISA;
        
#ifdef BRISANJE_ZASTARJELIH_LOG
        cout << "pokrenuto brisanje zastarjelih zapisa..." << endl;
        cout << "vrijeme: " << trenutno_vrijeme << endl;
#endif
        
        /* trazenje zastarijelih zapisa iz baze */
        for(opet=1, i=0, k=popis_dodavanja_zapisa.size(); opet && i<k; i++) {
            /* provjeravaju se samo zapisi stariji od trenutnog vremena */
            if(popis_dodavanja_zapisa[i].vrijeme < trenutno_vrijeme) {
                /* za brisanje iz baze potrebno je provjeriti vrijeme zapisa */
                if(baza[popis_dodavanja_zapisa[i].identifikator].vrijeme ==
                        popis_dodavanja_zapisa[i].vrijeme) {
                    /* vremena su ista => slijedi brisanje zapisa */
                    baza.erase(popis_dodavanja_zapisa[i].identifikator);
#ifdef INFO_PRIKAZ
                    cout << "izbrisan je zapis iz baze: " 
                         << popis_dodavanja_zapisa[i].identifikator
                         << endl;
#endif
                }
            } else
                opet=0;
        }
        
#ifdef BRISANJE_ZASTARJELIH_LOG
        cout << "zavrseno brisanje zastarjelih zapisa" << endl;
        cout << "broj obrisanih zapisa iz popisa(ne baze): " << i-1 << endl;
#endif
        
        /* brisanje zastarijelih vremenskih zapisa */
        popis_dodavanja_zapisa.erase(popis_dodavanja_zapisa.begin(), popis_dodavanja_zapisa.begin()+i);
        
        /* otkljucavanje pristupa bazi */
        m_baza.unlock();
    }
}

/* prepoznavanje tipa primljene poruke, pristup bazi i priprema odgovora */
bool obradi_poruku(sporuka poruka, unsigned char* buffer, sadresa adresa) {
    bool odg = false;
    switch(poruka.tip_poruke) {
        case MSG_PING: break;
        case MSG_PONG: break;
        case MSG_PONG_REG_REQ: break;
        
        case MSG_STREAM_ADVERTISEMENT: {
            /* zakljucaj pristup bazi */
            m_baza.lock();
            
            /* provjeri da li je zapis sa trazenim identifikatorom vec u bazi */
            baza_it = baza.find(
                        poruka.msg_stream_advertisement.identif_strujanja);
            
            /* ako ne postoji u bazi */
            if(baza_it == baza.end() ) {
                /* kreiraj novi zapis i dodaj ga u bazu*/
                szapis zapis;
                zapis.vrijeme = time(0);
                zapis.poruka = poruka;
                zapis.adresa = adresa;
                baza.insert(std::pair<uint64_t, szapis>(
                    poruka.msg_stream_advertisement.identif_strujanja, zapis) );
                
                svremenski_zapis vz;
                vz.identifikator = 
                    poruka.msg_stream_advertisement.identif_strujanja;
                vz.vrijeme = zapis.vrijeme;
                popis_dodavanja_zapisa.push_back(vz);
                
#ifdef INFO_PRIKAZ
                cout << "BAZA: Novi zapis dodan. ID: " 
                     << poruka.msg_stream_advertisement.identif_strujanja 
                     << endl;
#endif
                
                /* pripremi poruku za odgovor tipa MSG_STREAM_REGISTERED */
                sporuka odgovor;
                odgovor.tip_poruke = MSG_STREAM_REGISTERED;
                odgovor.msg_stream_registered.identif_strujanja = 
                    poruka.msg_stream_advertisement.identif_strujanja;
                odgovor.msg_stream_registered.TTL_u_sec = MIN_TTL_SEC;
                odgovor.msg_stream_registered.adresa = 
                    poruka.msg_stream_advertisement.adresa;
                
                /* spremi poruku u izlazni buffer */
                std::string p0 = pretvori_poruku_iz_strukture(odgovor);
                strcpy((char*)buffer, p0.c_str() );
                
                /* obavijesti da je potrebno poslati odgovor */
                odg = true;
                
            } else {
                /* zapis sa trazenim identifikatorom vec postoji u bazi,
                    provjera da li je i sadrzaj zapisa jednak */
                if(memcmp((void*)&(baza_it->second.poruka), 
                        (void*)&(poruka.msg_stream_advertisement), 
                        sizeof(sporuka) ) ) {
                    /* azuriraj vrijeme dodavanja zapisa */
                    baza_it->second.vrijeme = time(0);
                    
                    /* pripremi poruku za odgovor tipa MSG_STREAM_REGISTERED */
                    sporuka odgovor;
                    odgovor.tip_poruke = MSG_STREAM_REGISTERED;
                    odgovor.msg_stream_registered.identif_strujanja = 
                        baza_it->first;
                    odgovor.msg_stream_registered.TTL_u_sec = MIN_TTL_SEC;
                    odgovor.msg_stream_registered.adresa = 
                        baza_it->second.poruka.msg_stream_advertisement.adresa;
                    
                    svremenski_zapis vz;
                    vz.identifikator = 
                        poruka.msg_stream_advertisement.identif_strujanja;
                    vz.vrijeme = baza_it->second.vrijeme;
                    popis_dodavanja_zapisa.push_back(vz);
                    
#ifdef INFO_PRIKAZ
                    cout << "BAZA: Zapis vec postoji."
                         << "Osvjezeno je vrijeme zapisa. ID: "
                         << poruka.msg_stream_advertisement.identif_strujanja 
                         << endl;
#endif
                    
                    /* spremi poruku u izlazni buffer */
                    std::string p0 = pretvori_poruku_iz_strukture(odgovor);
                    strcpy((char*)buffer, p0.c_str() );
                    
                    /* obavijesti da je potrebno poslati odgovor */
                    odg = true;
                    
                } else {
                    /* sadrzaj nije jednak -> pripremi odgovor tipa MSG_INU */
                    sporuka odgovor;
                    odgovor.tip_poruke = MSG_IDENTIFIER_NOT_USABLE;
                    odgovor.msg_identifier_not_usable.identif_strujanja = 
                        baza_it->first;
                    odgovor.msg_identifier_not_usable.adresa = 
                        baza_it->second.poruka.msg_stream_advertisement.adresa;
                    
                    /* spremi poruku u izlazni buffer */
                    std::string p0 = pretvori_poruku_iz_strukture(odgovor);
                    strcpy((char*)buffer, p0.c_str() );
                    
                    /* obavijesti da je potrebno poslati odgovor */
                    odg = true;
                }
            }
            
            /* otkljucaj pristup bazi */
            m_baza.unlock();
            
            break;
        }
        
        case MSG_FIND_STREAM_SOURCE: {
            /* zakljucaj pristup bazi */
            m_baza.lock();
            
            /* provjeri da li je zapis sa trazenim identifikatorom u bazi */
            baza_it = baza.find(
                        poruka.msg_find_stream_source.identif_strujanja);
            if(baza_it != baza.end() ) {
                /* pripremi poruku za odgovor tipa MSG_STREAM_REGISTERED */
                sporuka odgovor;
                odgovor.tip_poruke = MSG_STREAM_SOURCE_DATA;
                odgovor.msg_stream_source_data.identif_strujanja = 
                    baza_it->first;
                odgovor.msg_stream_source_data.javna_adresa_reproduktora = 
                    baza_it->second.poruka.msg_stream_advertisement.adresa;
                odgovor.msg_stream_source_data.javna_adresa_izvora = 
                    baza_it->second.poruka.msg_stream_advertisement.adresa;
                odgovor.msg_stream_source_data.lokalna_adresa_izvora = 
                    baza_it->second.poruka.msg_stream_advertisement.adresa;
                
                /* spremi poruku u izlazni buffer */
                std::string p0 = pretvori_poruku_iz_strukture(odgovor);
                strcpy((char*)buffer, p0.c_str() );
                
                /* obavijesti da je potrebno poslati odgovor */
                odg = true;
            }
            
            /* otkljucaj pristup bazi */
            m_baza.unlock();
            
            /* ako zapis ne postoji poruka se ignorira/odbacuje */
            break;
        }
        
        case MSG_STREAM_REMOVE: {
            /* zakljucaj pristup bazi */
            m_baza.lock();
            
            /* provjeri da li je zapis sa trazenim identifikatorom vec u bazi */
            baza_it = baza.find(poruka.msg_stream_remove.identif_strujanja);
            if(baza_it != baza.end() && 
                    memcmp((void*)&(baza_it->second.adresa), 
                    (void*)&adresa, sizeof(sadresa) ) ) {
                baza.erase(baza_it->first);
            }
            
            /* otkljucaj pristup bazi */
            m_baza.unlock();
            
            /* inace ignoriraj poruku */
            break;
        }
        
        case MSG_REQ_RELAY_LIST: {
            strcpy((char*)buffer, poruka_popis_relaya_def.c_str() );
            
            /* obavijesti da je potrebno poslati odgovor */
            odg = true;
            
            break;
        }
        
        default:
#ifdef INFO_PRIKAZ
            cout << "Primljena poruka nije poznatog tipa!" << endl;
#endif
            break;
    }
    
    /* false = nije potreban odgovor, true = potreban je odgovor */
    return odg;
}

/* informacija da li program jos uvijek treba raditi */
bool radi = true;

/* pristup i obrada primljene poruke */
std::mutex provjera_poruke;
std::condition_variable dostupna_nova_poruka;
std::condition_variable dostupni_prazni_buffer;

/* zapis pristigle poruke */
struct szapis_poruke {
    unsigned char poruka[MAX_BUFFER_LOCAL_SIZE];
    sockaddr_in addr;
    socklen_t addr_len;
    //szapis_poruke() { poruka = new unsigned char[MAX_BUFFER_SIZE]; }
    //~szapis_poruke() { delete[] poruka; }
};

/* red buffer-a */
std::vector<szapis_poruke> red_buffera;

/* redovi id-ova bufera na cekanju i u pripremi */
std::queue<int> red_punih;
std::queue<int> red_praznih;

/* dretva za prihvat poruka */
void prihvati_poruku() {
    /* alokacija varijabli vezanih uz komunikaciju */
    int sz;
    
    /* id buffera u koji s sprema poruka */
    int id_buffera;
    
#ifdef INFO_PRIKAZ
    cout << "Dretva za prihvat poruka je pokrenuta..." << endl;
#endif
    
    while(radi) {
        {
            /* zakljucaj pristup redu praznih */
            std::unique_lock<std::mutex> zakljucano(monitor);
            while(red_praznih.empty() )
                dostupni_prazni_buffer.wait(zakljucano);
            
            /* preuzmi id buffera iz reda praznih */
            id_buffera = red_praznih.front();
            red_praznih.pop();
            
            /* otkljucaj pristup redu punih */
        }
        
        red_buffera[id_buffera].addr_len = sizeof(red_buffera[id_buffera].addr);
        
        /* cekanje na poruku */
        sz = recvfrom(sock_id, (void*)&red_buffera[id_buffera].poruka,
                        MAX_BUFFER_SIZE, 0,
                        (sockaddr*)&red_buffera[id_buffera].addr,
                        &red_buffera[id_buffera].addr_len);
        
        if(sz>0) {
            /* ispis informacija o primljenoj poruci */
#ifdef INFO_PRIKAZ
            cout << "Dretva za cekanje poruka: Poruka primljena." << endl;
            cout << "primljeno: [";
            for(int i=0; i<strlen((char*)red_buffera[id_buffera].poruka); i++)
                cout << (int)red_buffera[id_buffera].poruka[i] << " ";
            cout << "] ..." << endl;
#endif
            
            /* zakljucaj pristup redu praznih */
            monitor.lock();
            
            /* dodaj koristeni id buffera u red punih */
            red_punih.push(id_buffera);
            
            /* obavijesti jednu dretvu za obradu poruke */
            dostupna_nova_poruka.notify_one();
            
            /* otkljucaj pristup redu praznih */
            monitor.unlock();
        }
    }
}

/* dretva koju izvodi svaka od dretvi i sluzi za prihvacanje poruke, te
   ako je potrebno - vracanje odgovora na poruku, nakon toga dretva ide
   u red cekanja i ceka na svoj red za obradu nove poruke
*/
void obradi_zahtjev(int id) {
    /* alokacija varijabli vezanih uz komunikaciju */
    int sz;
    
    /* oznaka da li poruka zahtjeva odgovor */
    bool treba_odgovor;
    
#ifdef INFO_PRIKAZ
    cout << "Dretva (" << id << "): Pokrenuta i spremna za rad..." << endl;
#endif
    
    /* pomocni objekt za privremenu pohranu poruke koju dretva obradjuje */
    int id_buffera;
    
    /* svaka dretva ponavlja:
        0. cekaj na poruku,
        1. preuzmi poruku (K.O.),
        2. obradi poruku
        3. posalji odgovor (neke poruke ne zahtjevaju ovu akciju)
    */
    while(radi) {
        {
            /* zakljucaj pristup redu punih */
            std::unique_lock<std::mutex> zakljucano(monitor);
            while(red_punih.empty() )
                dostupna_nova_poruka.wait(zakljucano);
            
            /* preuzmi id buffera iz reda punih */
            id_buffera = red_punih.front();
            red_punih.pop();
            
            /* otkljucaj pristup redu punih */
        }
        
        /* ispis informacija o primljenoj poruci */
#ifdef INFO_PRIKAZ
        cout << "obrada primljene poruke: [";
        for(int i=0; i<strlen((char*)red_buffera[id_buffera].poruka); i++)
            cout << (int)red_buffera[id_buffera].poruka[i] << " ";
        cout << "] ..." << endl;
#endif
        
        /* pretvori poruku u oblik za obradu */
        sporuka poruka = pretvori_poruku_u_strukturu(
                                (unsigned char*)red_buffera[id_buffera].poruka,
                                strlen((char*)red_buffera[id_buffera].poruka) );
        
        /* resetiraj buffer */
        memset(red_buffera[id_buffera].poruka, 0, MAX_BUFFER_SIZE);
        
        /* pripremi odgovor na poruku ako je potreban */
        sadresa adresa;
        adresa.tip_adrese = ADR_IPv4;
        adresa.adr_ipv4 = red_buffera[id_buffera].addr.sin_addr.s_addr;
        treba_odgovor = obradi_poruku(poruka, red_buffera[id_buffera].poruka, adresa);
        
        /* ukoliko je potrebno, vratiti odgovor na poruku */
        if(treba_odgovor) {
            sz = sendto(sock_id, (void*)&red_buffera[id_buffera].poruka, MAX_BUFFER_SIZE, 0,
                        (sockaddr*)&red_buffera[id_buffera].addr, red_buffera[id_buffera].addr_len);
            
#ifdef INFO_PRIKAZ
            cout << "saljem: [";
            for(int i=0; i<strlen((char*)red_buffera[id_buffera].poruka); i++)
                cout << (int)red_buffera[id_buffera].poruka[i] << " ";
            cout << "] prema : " << red_buffera[id_buffera].addr_len
                 << " :: " << inet_ntoa(red_buffera[id_buffera].addr.sin_addr) << endl;
#endif
        }
        
        /* zakljucaj pristup redu praznih */
        monitor.lock();
        
        /* dodaj koristeni id buffera u red praznih */
        red_praznih.push(id_buffera);
            
        /* otkljucaj pristup redu praznih */
        monitor.unlock();
        
        /* obavijesti da je jedan buffer prazan */
        dostupni_prazni_buffer.notify_one();
    }
}

/* bazen dretvi */
std::vector<std::thread> bazen_dretvi;

/* dretva za prihvat poruke */
std::thread prihvat;

/* dretva za brisanje zastarjelih zapisa */
std::thread zastarjeli;

/******************************************************************************/
/** PREKIDNA RUTINA                                                          **/
/******************************************************************************/

/* prekidna rutina */
void pocisti_sve_kod_prekida(int signum) {
    cout << "Prekid rada procesa" << endl;
    
    /* oznaka prekida rada dretve */
    radi = false;
    
#ifdef INFO_LOG
    cout << "Testiranje za potrebom prekida veze..." << endl;
#endif
    
    /* zatvori vezu */
    if(sock_id > 0) {
        /* zatvaranje veze ako je postojala */
        close(sock_id);
#ifdef INFO_LOG
    cout << "Veza zatvorena." << endl;
#endif
    } else {
#ifdef INFO_LOG
        cout << "Veza nije bila otvorena." << endl;
#endif
    }
    
#ifdef INFO_LOG
    cout << "Prekidanje dretvi..." << endl;
#endif
    
    /* prekid svih dretvi iz bazena dretvi */
    zastarjeli.detach();
    prihvat.detach();
    for(std::thread& t : bazen_dretvi)
        t.detach();
    
    /* brisanje svih dretvi iz spremnika */
    bazen_dretvi.clear();
    
#ifdef INFO_LOG
    cout << "Proces je uspjesno prekinut." << endl;
#endif
    
    /* prekid programa */
    exit(signum);
}

/******************************************************************************/
/** MAIN                                                                     **/
/******************************************************************************/

int main() {
    /* pripremi prekidnu rutinu */
    signal(SIGINT, pocisti_sve_kod_prekida);
    
    /* ucitava parametre iz konfiguracijske datoteke */
    ucitaj_konfig_podatke();
    
    /* odredi fiksnu velicinu za polje buffer-a(std::vector) */
    red_buffera.reserve(MAX_N_THREAD+1);
    szapis_poruke zapis;
    for(int i=0; i<MAX_N_THREAD+1; i++) {
        red_buffera.push_back(zapis);
    }
    
    /* popuni red praznih */
    for(int i=0; i<MAX_N_THREAD+1; i++) {
        red_praznih.push(i);
    }
    
    /* podaci servera */
    sock_id = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_id<0) {
#ifdef ERROR_LOG
        cout << "Greska: nedostupan socket." << endl;
#endif
        exit(-1);
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERV_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERV_IP_ADDRESS.c_str() );
    
    /* otvori vezu */
#ifdef INFO_LOG
    cout << "Otvaranje veze..." << endl;
#endif
    if(bind(sock_id, (sockaddr*)&server_addr, sizeof(server_addr) ) ) {
#ifdef ERROR_LOG
    cout << "Greska" << endl;
#endif
        exit(-1);
    }
#ifdef INFO_LOG
    cout << "Veza otvorena" << endl;
#endif
    
    /* ucitaj popis relay-a iz datoteke */
    ucitaj_relaye();
    provjeri_dostupnost_relaya();
    
    /* priprema bazena dretvi */
#ifdef INFO_LOG
    cout << "Priprema bazena dretvi..." << endl;
#endif
    
    /* pokretanje svih dretvi */
    for(int i=0; i<MAX_N_THREAD; i++)
        bazen_dretvi.push_back(std::thread(obradi_zahtjev, i) );
    
    /* dretva za prihvat poruke */
    prihvat = std::thread(prihvati_poruku);
    
    /* dretva za brisanje zastarjelih zapisa */
    zastarjeli = std::thread(brisi_zastarjeli);
    
#ifdef INTERAKCIJA
    /* interakcija na serverskoj strani */
    std::string x;
    usleep(200000);
    while(1) {
        /* izbornik */
        cout << "-------------------------------" << endl;
        cout << " [baza] - ispis sadrzaja baze" << endl;
        cout << " [prekid] - prekid servera" << endl;
        cout << "------------------------------" << endl;
        
        /* cekanje naredbe */
        getline(cin, x);
        
        /* odredjivanje akcije prema odabranoj naredbi */
        if(x == "baza") {
            /* zakljucavanje pristupa bazi */
            m_baza.lock();
            
            /* ispis podataka iz baze */
            cout << endl << "sadrzaj baze:" << endl;
            
            /* testiraj da li je baza prazna */
            if(baza.empty() ) {
                cout << "-prazno-" << endl;
            } else {
                /* prolaz kroz sve zapise u bazi */
                for(auto b : baza) {
                    cout << b.first << endl;
                }
            }
            cout << endl;
            
            /* otkljucavanje pristupa bazi */
            m_baza.unlock();
        } else if(x == "prekid") {
#ifdef INFO_LOG
            cout << "prekid programa" << endl;
#endif
            pocisti_sve_kod_prekida(0);
        } else {
#ifdef ERROR_LOG
            cout << "nepoznata naredba" << endl;
#endif
        }
    }
#endif /* INTERAKCIJA */
    
    /* cekanje dretvi */
    zastarjeli.join();
    prihvat.join();
    for(std::thread& t : bazen_dretvi)
        t.join();
    
    /* zatvori vezu */
    close(sock_id);
#ifdef INFO_LOG
    cout << "Veza zatvorena" << endl;
#endif
    
    /* zavrsetak programa */
    return 0;
}

/**
 **    Popis glavnih dijelova programa:
 **    (A) prihvaćanje poruke i pokretanje dretve za obradu poruke
 **    (B) slanje odgovora na poruku iz dretve nakon obrade poruke
 **    (C) učitavanje konfiguracijske datoteke i opis strukture datoteke
 **    (D) prepoznavanje vrste poruke i obrada poruke
 **    (E) rad s bazom (struktura tipa std::map koja sadrži primljene zahtjeve
 **    (F) testiranje servera (opis eventualnih unaprijeđenja, neke 
 **     karakteristicne točke programa, brzina ili mjerilo optimiziranosti
 **     programskog koda)
 **/

/*
 *  Preostalo za dodati/prepraviti:
 *  
 *  kod:
 *      - funkcija za prevorbu 'iz poruke u strukturu'              (ODRADJENO)
 *      - funkcija za pretvorbu 'iz strukture u poruku'             (ODRADJENO)
 *      - dodavanje/brisanje iz baze (std::map)                     (ODRADJENO)
 *      - test sa klijentom                                               (95%)
 *      - prekidnu rutinu za ciscenje memorije                      (ODRADJENO)
 *      - dodati ping-pong request za relay-ove                      (NE-TREBA)
 *      - dodati mogucnost za IPv6? (*)                                   (DIO)
 *      - dodati posebnu dretvu za primanje poruke                  (ODRADJENO)
 *      - dodati vremensko brisanje kljuca iz baze                  (ODRADJENO)
 *  
 *  v17:
 *      - promjena std::map u std::unordered_map                    (ODRADJENO)
 *      - promjena ucitavanja relay-a (koristi funkcije inet_*)     (ODRADJENO)
 *      - promjena broj u mrezni zapis                                  (ALPHA)
 *  
 *  test:
 *      - ispravno odredjivanje tipa poruke                         (ODRADJENO)
 *      - testni kod klijenta                                           (ALPHA)
 *      - testirati pravilnu pretvorbu (ntoh, hton)                        (0%)
 *      - testirati inet_* funkcije - popis relay-a                      (RADI)
 */

/*****************************************************************************
 *  Original name:  Posluzitelj za pronalazenje
 *  
 *  Authors:        Matija Belec
 *                  Dario Grubesic
 *                  Mario Hudincec
 *                  Kristijan Jedvaj
 *                  Vibor Kovacic
 *                  Marko Lesnicar
 *                  
 *  Copyright:      Copyright (c) 2014-2015 by authors. All Rights Reserved.
 ****************************************************************************/
