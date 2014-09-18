/*
    Socle - Socket Library Ecosystem
    Copyright (c) 2014, Ales Stibal <astib@mag0.net>, All rights reserved.

    This library  is free  software;  you can redistribute  it and/or
    modify  it  under   the  terms of the  GNU Lesser  General Public
    License  as published by  the   Free Software Foundation;  either
    version 3.0 of the License, or (at your option) any later version.
    This library is  distributed  in the hope that  it will be useful,
    but WITHOUT ANY WARRANTY;  without  even  the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
    
    See the GNU Lesser General Public License for more details.
    
    You  should have received a copy of the GNU Lesser General Public
    License along with this library.
*/

#ifndef __SSLCERTSTORE_HPP__
#define __SSLCERTSTORE_HPP__

#include <map>

#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <logger.hpp>

#include <thread>

/* define HOME to be dir for key and cert files... */
#define HOME "./certs/"
/* Make these what you want for cert & key files */
#define CL_CERTF  "cl-cert.pem"
#define CL_KEYF   "cl-key.pem"
#define SR_CERTF  "srv-cert.pem"
#define SR_KEYF   "srv-key.pem"

#define CA_CERTF  "ca-cert.pem"
#define CA_KEYF   "ca-key.pem"

typedef std::pair<EVP_PKEY*,X509*> X509_PAIR;
typedef std::map<std::string,X509_PAIR*> X509_CACHE;


class SSLCertStore {
   
public:
    
    int       serial=0xCABA1A;
    
    X509*     ca_cert = nullptr; // ca certificate
    EVP_PKEY* ca_key = nullptr;  // ca key to self-sign 
    
    X509*     def_sr_cert = nullptr; // default server certificate
    EVP_PKEY* def_sr_key = nullptr;  // default server key
    
    X509*     def_cl_cert = nullptr;  // default client certificate
    EVP_PKEY* def_cl_key = nullptr;   // default client key

    static std::string certs_path;
    static std::string password;
    
    static int password_callback(char* buf, int size, int rwflag, void*u);
    
    bool load();
        bool load_ca_cert();
        bool load_def_cl_cert();
        bool load_def_sr_cert();
    
    void destroy();
    
     X509_CACHE cache_;
     std::mutex mutex_cache_write_;

     // our killer feature here 
     X509_PAIR* spoof(X509* cert);
     
     static int convert_ASN1TIME(ASN1_TIME*, char*, size_t);
     static std::string print_cert(X509*);
     
     bool add(std::string& subject, EVP_PKEY* cert_privkey,X509* cert,X509_REQ* req=NULL);
     bool add(std::string& subject, X509_PAIR* p,X509_REQ* req=NULL);
     
     X509_PAIR* find(std::string& subject);
     void erase(std::string& subject);
     
     virtual ~SSLCertStore();

};

#endif //__SSLCERTSTORE_HPP__