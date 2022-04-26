// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>

#include <vm/runtime.h>

#include <object/datatype/error.h>
#include <object/datatype/integer.h>

#include <module/modules.h>

#include "ssl.h"

using namespace argon::object;
using namespace argon::module;
using namespace argon::module::ssl;

ARGON_ERROR_TYPE_SIMPLE(SSLError, "", argon::module::ssl::type_ssl_error_);

ArObject *argon::module::ssl::SSLErrorGet() {
    char buf[256] = {};

    if (ERR_error_string(ERR_get_error(), buf) == nullptr)
        return ErrorFormatNoPanic(type_os_error_, "unknown error");

    return ErrorNew(type_ssl_error_, buf);
}

ArObject *argon::module::ssl::SSLErrorGet(const SSLSocket *socket, int ret) {
    auto sslerr = SSL_get_error(socket->ssl, ret);
    auto errcode = ERR_peek_last_error();
    const char *errmsg;
    ArObject *retobj;
    Tuple *tp;

    switch (sslerr) {
        case SSL_ERROR_ZERO_RETURN:
            errmsg = "TLS/SSL connection has been closed (EOF)";
            break;
        case SSL_ERROR_WANT_READ:
            errmsg = "the read operation did not complete";
            break;
        case SSL_ERROR_WANT_WRITE:
            errmsg = "the write operation did not complete";
            break;
        case SSL_ERROR_WANT_X509_LOOKUP:
            errmsg = "the X509 lookup operation did not complete";
            break;
        case SSL_ERROR_WANT_CONNECT:
            errmsg = "the connect operation did not complete";
            break;
        case SSL_ERROR_SYSCALL:
            if (errcode == 0) {
                if (ret == 0)
                    errmsg = "EOF occurred in violation of protocol";
                else if (ret == -1) {
                    ERR_clear_error();
#ifdef _ARGON_PLATFORM_WINDOWS
                    if (ErrorGetLast() != 0)
                        return ErrorSetFromWinError();
#endif
                    if (errno != 0)
                        return ErrorSetFromErrno();
                    else
                        errmsg = "EOF occurred in violation of protocol";
                } else
                    errmsg = "unknown I/O error occurred";
            } else
                errmsg = "SSL syscall error";
            break;
        case SSL_ERROR_SSL:
            errmsg = "failure in the SSL library occurred";
            if (ERR_GET_LIB(errcode) == ERR_LIB_SSL &&
                ERR_GET_REASON(errcode) == SSL_R_CERTIFICATE_VERIFY_FAILED)
                errmsg = "failure in the certificate verify";
            break;
        default:
            errmsg = "invalid error";
            break;
    }

    ERR_clear_error();

    if ((tp = TupleNew("Is", sslerr, errmsg)) == nullptr)
        return nullptr;

    retobj = ErrorNew(type_ssl_error_, tp);
    Release(tp);

    return retobj;
}

ArObject *argon::module::ssl::SSLErrorSet() {
    ArObject *err = SSLErrorGet();

    if (err != nullptr) {
        argon::vm::Panic(err);
        Release(err);
    }

    return nullptr;
}

ArObject *argon::module::ssl::SSLErrorSet(const SSLSocket *socket, int ret) {
    ArObject *err = SSLErrorGet(socket, ret);

    if (err != nullptr) {
        argon::vm::Panic(err);
        Release(err);
    }

    return nullptr;
}

Bytes *argon::module::ssl::CertToDer(X509 *cert) {
    unsigned char *buf;
    Bytes *ret;
    int len;

    if ((len = i2d_X509(cert, &buf)) < 0)
        return (Bytes *) SSLErrorSet();

    ret = BytesNew(buf, len, true);

    OPENSSL_free(buf);

    return ret;
}

static ArObject *ASN1Obj2Ar(const ASN1_OBJECT *name) {
#define X509_NAME_MAXLEN  256

    char buf[X509_NAME_MAXLEN];
    char *namebuf = buf;
    String *aname;
    int buflen;

    if ((buflen = OBJ_obj2txt(namebuf, X509_NAME_MAXLEN, name, 0)) < 0) {
        SSLErrorSet();
        return nullptr;
    }

    if (buflen > X509_NAME_MAXLEN - 1) {
        buflen = OBJ_obj2txt(nullptr, 0, name, 0);

        if ((namebuf = ArObjectNewRaw<char *>(buflen + 1)) == nullptr)
            return nullptr;

        if ((buflen = OBJ_obj2txt(namebuf, X509_NAME_MAXLEN, name, 0)) < 0) {
            argon::memory::Free(namebuf);
            SSLErrorSet();
            return nullptr;
        }
    }

    if (buf != namebuf) {
        if ((aname = StringNewBufferOwnership((unsigned char *) namebuf, buflen)) == nullptr) {
            argon::memory::Free(namebuf);
            return nullptr;
        }
    } else
        aname = StringNew(namebuf, buflen);

    return aname;

#undef X509_NAME_MAXLEN
}

static Tuple *Attribute2Tuple(const ASN1_OBJECT *name, const ASN1_STRING *value) {
    ArObject *asn1obj = ASN1Obj2Ar(name);
    String *tmp;
    Tuple *retval;

    ArSSize buflen;

    if (asn1obj == nullptr)
        return nullptr;

    if (ASN1_STRING_type(value) == V_ASN1_BIT_STRING) {
        buflen = ASN1_STRING_length(value);

        if ((tmp = StringNew((const char *) ASN1_STRING_get0_data(value), buflen)) == nullptr) {
            Release(asn1obj);
            return nullptr;
        }
    } else {
        unsigned char *buf;

        if ((buflen = ASN1_STRING_to_UTF8(&buf, value)) < 0) {
            SSLErrorSet();
            Release(asn1obj);
            return nullptr;
        }

        if ((tmp = StringNew((const char *) buf, buflen)) == nullptr) {
            Release(asn1obj);
            return nullptr;
        }

        OPENSSL_free(buf);
    }

    retval = TupleNew("aa", asn1obj, tmp);
    Release(asn1obj);

    return retval;
}

static Tuple *TupleX509Name(const X509_NAME *name) {
    const X509_NAME_ENTRY *entry;
    ASN1_OBJECT *as_name;
    ASN1_STRING *value;

    ArObject *tmp;
    List *dn;
    List *rdn;
    Tuple *ret;

    int entry_count = X509_NAME_entry_count(name);
    int rdn_level = -1;

    if ((dn = ListNew()) == nullptr)
        return nullptr;

    if ((rdn = ListNew()) == nullptr) {
        Release(dn);
        return nullptr;
    }

    for (int i = 0; i < entry_count; i++) {
        entry = X509_NAME_get_entry(name, i);

        if (rdn_level >= 0 && rdn_level != X509_NAME_ENTRY_set(entry)) {
            if ((ret = TupleNew(rdn)) == nullptr) {
                Release(dn);
                Release(rdn);
                return nullptr;
            }

            Release(rdn);

            if (!ListAppend(dn, ret)) {
                Release(dn);
                Release(ret);
                return nullptr;
            }

            Release((ArObject **) &ret);

            if ((rdn = ListNew()) == nullptr) {
                Release(dn);
                return nullptr;
            }
        }

        rdn_level = X509_NAME_ENTRY_set(entry);

        as_name = X509_NAME_ENTRY_get_object(entry);
        value = X509_NAME_ENTRY_get_data(entry);

        if ((tmp = Attribute2Tuple(as_name, value)) == nullptr) {
            Release(dn);
            Release(rdn);
            return nullptr;
        }

        if (!ListAppend(rdn, tmp)) {
            Release(dn);
            Release(rdn);
            Release(tmp);
            return nullptr;
        }

        Release(tmp);
    }

    if (rdn->len > 0) {
        if ((ret = TupleNew(rdn)) == nullptr) {
            Release(dn);
            Release(rdn);
            return nullptr;
        }

        Release(rdn);

        if (!ListAppend(dn, ret)) {
            Release(dn);
            Release(ret);
            return nullptr;
        }

        Release((ArObject **) &ret);
    }

    Release(rdn);

    ret = TupleNew(dn);
    Release(dn);

    return ret;
}

static ArObject *SubjectAltName(const X509 *cert) {
    char buf[2048];
    List *alt_names = nullptr;
    GENERAL_NAMES *names;
    BIO *biobuf;

    Tuple *ret;

    if (cert == nullptr)
        return ARGON_OBJECT_NIL;

    if ((biobuf = BIO_new(BIO_s_mem())) == nullptr) {
        // TODO: err?!
        return nullptr;
    }

    names = (GENERAL_NAMES *) X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr);
    if (names != nullptr) {
        Tuple *tuple_tmp = nullptr;

        if ((alt_names = ListNew()) == nullptr) {
            BIO_free(biobuf);
            return nullptr;
        }

        for (int i = 0; i < sk_GENERAL_NAME_num(names); i++) {
            const ASN1_STRING *as;
            GENERAL_NAME *name;

            ArObject *tmp;
            String *str_tmp;
            String *str_tmp1;

            const char *vptr;
            int len;

            tuple_tmp = nullptr;

            name = sk_GENERAL_NAME_value(names, i);

            if (name->type == GEN_DIRNAME) {
                if ((tmp = TupleX509Name(name->d.dirn)) == nullptr)
                    break;

                tuple_tmp = TupleNew("sa", "DirName", tmp);
                Release(tmp);
            } else if (name->type == GEN_EMAIL) {
                as = name->d.rfc822Name;
                if ((str_tmp = StringNew((const char *) ASN1_STRING_get0_data(as), ASN1_STRING_length(as))) == nullptr)
                    break;

                tuple_tmp = TupleNew("sa", "email", str_tmp);
                Release(str_tmp);
            } else if (name->type == GEN_DNS) {
                as = name->d.dNSName;
                if ((str_tmp = StringNew((const char *) ASN1_STRING_get0_data(as), ASN1_STRING_length(as))) == nullptr)
                    break;

                tuple_tmp = TupleNew("sa", "DNS", str_tmp);
                Release(str_tmp);
            } else if (name->type == GEN_URI) {
                as = name->d.uniformResourceIdentifier;
                if ((str_tmp = StringNew((const char *) ASN1_STRING_get0_data(as), ASN1_STRING_length(as))) == nullptr)
                    break;

                tuple_tmp = TupleNew("sa", "URI", str_tmp);
                Release(str_tmp);
            } else if (name->type == GEN_RID) {
                if ((len = i2t_ASN1_OBJECT(buf, sizeof(buf) - 1, name->d.rid)) < 0) {
                    SSLErrorSet();
                    break;
                }

                if (len >= sizeof(buf))
                    str_tmp = StringNew("<INVALID>");
                else
                    str_tmp = StringNew(buf, len);

                if (str_tmp == nullptr)
                    break;

                tuple_tmp = TupleNew("sa", "Registered ID", str_tmp);
                Release(str_tmp);
            } else if (name->type == GEN_IPADD) {
                const unsigned char *ip = name->d.ip->data;

                if (name->d.ip->length == 4)
                    str_tmp = StringNewFormat("%d.%d.%d.%d",
                                              ip[0], ip[1], ip[2], ip[3]);
                else if (name->d.ip->length == 6)
                    str_tmp = StringNewFormat("%X:%X:%X:%X:%X:%X:%X:%X",
                                              ip[0] << 8u | ip[1],
                                              ip[2] << 8u | ip[3],
                                              ip[4] << 8u | ip[5],
                                              ip[6] << 8u | ip[7],
                                              ip[8] << 8u | ip[9],
                                              ip[10] << 8u | ip[11],
                                              ip[12] << 8u | ip[13],
                                              ip[14] << 8u | ip[15]);
                else
                    str_tmp = StringNew("<INVALID>");

                tuple_tmp = TupleNew("sa", "IP Address", str_tmp);
                Release(str_tmp);
            } else {
                if (name->type != GEN_OTHERNAME &&
                    name->type != GEN_X400 &&
                    name->type != GEN_EDIPARTY &&
                    name->type != GEN_RID) {
                    ErrorFormat(type_runtime_error_, "unknown general name type %d", name->type);
                    break;
                }

                BIO_reset(biobuf);
                GENERAL_NAME_print(biobuf, name);
                if ((len = BIO_gets(biobuf, buf, sizeof(buf) - 1)) < 0) {
                    SSLErrorSet();
                    break;
                }

                if ((vptr = strchr(buf, ':')) == nullptr) {
                    ErrorFormat(type_value_error_, "invalid value %.200s", buf);
                    break;
                }

                if ((str_tmp = StringNew(buf, (vptr - buf))) == nullptr)
                    break;

                if ((str_tmp1 = StringNew((vptr + 1), len - (vptr - buf + 1))) == nullptr) {
                    Release(str_tmp);
                    break;
                }

                tuple_tmp = TupleNew("aa", str_tmp, str_tmp1);
                Release(str_tmp);
                Release(str_tmp1);
            }

            if (!ListAppend(alt_names, tuple_tmp)) {
                Release(tuple_tmp);
                break;
            }

            Release(tuple_tmp);
        }

        sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);

        if (tuple_tmp == nullptr) {
            BIO_free(biobuf);
            Release(alt_names);
            return nullptr;
        }
    }

    BIO_free(biobuf);

    if (alt_names != nullptr) {
        ret = TupleNew(alt_names);
        Release(alt_names);
        return ret;
    }

    return ARGON_OBJECT_NIL;
}

static ArObject *AiaURI(const X509 *cert, int nid) {
    AUTHORITY_INFO_ACCESS *info;
    String *tmp;
    List *list;
    Tuple *ret;

    info = (AUTHORITY_INFO_ACCESS *) X509_get_ext_d2i(cert, NID_info_access, nullptr, nullptr);
    if (info == nullptr)
        return ARGON_OBJECT_NIL;

    if (sk_ACCESS_DESCRIPTION_num(info) == 0) {
        AUTHORITY_INFO_ACCESS_free(info);
        return ARGON_OBJECT_NIL;
    }

    if ((list = ListNew()) == nullptr) {
        AUTHORITY_INFO_ACCESS_free(info);
        return nullptr;
    }

    for (int i = 0; i < sk_ACCESS_DESCRIPTION_num(info); i++) {
        const ACCESS_DESCRIPTION *ad = sk_ACCESS_DESCRIPTION_value(info, i);
        const ASN1_IA5STRING *uri;

        if ((OBJ_obj2nid(ad->method) != nid) || (ad->location->type != GEN_URI))
            continue;

        uri = ad->location->d.uniformResourceIdentifier;

        if ((tmp = StringNew((const char *) uri->data, uri->length)) == nullptr) {
            AUTHORITY_INFO_ACCESS_free(info);
            Release(list);
            return nullptr;
        }

        if (!ListAppend(list, tmp)) {
            AUTHORITY_INFO_ACCESS_free(info);
            Release(list);
            return nullptr;
        }

        Release(tmp);
    }

    AUTHORITY_INFO_ACCESS_free(info);

    ret = TupleNew(list);
    Release(list);

    return ret;
}

static ArObject *DistributionPoints(const X509 *cert) {
    STACK_OF(DIST_POINT) *dps;
    List *list;
    Tuple *ret;

    dps = (STACK_OF(DIST_POINT) *) X509_get_ext_d2i(cert, NID_crl_distribution_points, nullptr, nullptr);

    if (dps == nullptr)
        return ARGON_OBJECT_NIL;

    if ((list = ListNew()) == nullptr)
        return nullptr;

    for (int i = 0; i < sk_DIST_POINT_num(dps); i++) {
        DIST_POINT *dp;
        STACK_OF(GENERAL_NAME) *gns;

        dp = sk_DIST_POINT_value(dps, i);
        if (dp->distpoint == nullptr)
            continue;

        gns = dp->distpoint->name.fullname;

        for (int j = 0; j < sk_GENERAL_NAME_num(gns); j++) {
            const GENERAL_NAME *gn;
            const ASN1_IA5STRING *uri;
            String *tmp;

            gn = sk_GENERAL_NAME_value(gns, j);
            if (gn->type != GEN_URI)
                continue;

            uri = gn->d.uniformResourceIdentifier;
            if ((tmp = StringNew((const char *) uri->data, uri->length)) == nullptr) {
                Release(list);
                CRL_DIST_POINTS_free(dps);
                return nullptr;
            }

            if (!ListAppend(list, tmp)) {
                Release(list);
                Release(tmp);
                CRL_DIST_POINTS_free(dps);
                return nullptr;
            }

            Release(tmp);
        }
    }

    ret = TupleNew(list);
    Release(list);

    return ret;
}

argon::object::Map *argon::module::ssl::DecodeCert(X509 *cert) {
    char buf[2048];
    ArObject *tmp;
    String *str_tmp;
    Map *ret;
    BIO *biobuf;

    int len;

    if ((ret = MapNew()) == nullptr)
        return nullptr;

    // PEER

    if ((tmp = TupleX509Name(X509_get_subject_name(cert))) == nullptr) {
        Release(ret);
        return nullptr;
    }

    if (!MapInsertRaw(ret, "subject", tmp))
        goto ERROR;

    Release(&tmp);

    // ISSUER

    if ((tmp = TupleX509Name(X509_get_issuer_name(cert))) == nullptr) {
        Release(ret);
        return nullptr;
    }

    if (!MapInsertRaw(ret, "issuer", tmp))
        goto ERROR;

    Release(&tmp);

    // VERSION

    if ((tmp = IntegerNew(X509_get_version(cert) + 1)) == nullptr)
        goto ERROR;

    if (!MapInsertRaw(ret, "version", tmp))
        goto ERROR;

    Release(&tmp);

    if ((biobuf = BIO_new(BIO_s_mem())) == nullptr)
        goto ERROR;

    // SERIAL NUMBER

    BIO_reset(biobuf);
    i2a_ASN1_INTEGER(biobuf, X509_get_serialNumber(cert));
    if ((len = BIO_gets(biobuf, buf, sizeof(buf) - 1)) < 0) {
        SSLErrorSet();
        goto ERROR;
    }

    if ((str_tmp = StringNew(buf, len)) == nullptr)
        goto ERROR;

    if (!MapInsertRaw(ret, "serialNumber", str_tmp))
        goto ERROR;

    Release((ArObject **) &str_tmp);

    // NOT BEFORE

    BIO_reset(biobuf);
    ASN1_TIME_print(biobuf, X509_get0_notBefore(cert));
    if ((len = BIO_gets(biobuf, buf, sizeof(buf) - 1)) < 0) {
        SSLErrorSet();
        goto ERROR;
    }

    if ((str_tmp = StringNew(buf, len)) == nullptr)
        goto ERROR;

    if (!MapInsertRaw(ret, "notBefore", str_tmp))
        goto ERROR;

    Release((ArObject **) &str_tmp);

    // NOT AFTER

    BIO_reset(biobuf);
    ASN1_TIME_print(biobuf, X509_get0_notAfter(cert));
    if ((len = BIO_gets(biobuf, buf, sizeof(buf) - 1)) < 0) {
        SSLErrorSet();
        goto ERROR;
    }

    if ((str_tmp = StringNew(buf, len)) == nullptr)
        goto ERROR;

    if (!MapInsertRaw(ret, "notAfter", str_tmp))
        goto ERROR;

    Release((ArObject **) &str_tmp);

    BIO_free(biobuf);

    if ((tmp = SubjectAltName(cert)) == nullptr)
        goto ERROR;

    if (!MapInsertRaw(ret, "subjectAltName", tmp))
        goto ERROR;

    Release(&tmp);

    if ((tmp = AiaURI(cert, NID_ad_OCSP)) == nullptr)
        goto ERROR;

    if (!MapInsertRaw(ret, "OCSP", tmp))
        goto ERROR;

    Release(&tmp);

    if ((tmp = AiaURI(cert, NID_ad_ca_issuers)) == nullptr)
        goto ERROR;

    if (!MapInsertRaw(ret, "caIssuers", tmp))
        goto ERROR;

    Release(&tmp);

    if ((tmp = DistributionPoints(cert)) == nullptr)
        goto ERROR;

    if (!MapInsertRaw(ret, "crlDistributionPoints", tmp))
        goto ERROR;

    Release(tmp);

    return ret;

    ERROR:
    return nullptr;
}

#ifdef _ARGON_PLATFORM_WINDOWS

ARGON_FUNCTION5(ssl_, enumcerts_windows, "", 1, false) {
    if (!CheckArgs("s:store_name", func, argv, count))
        return nullptr;

    return EnumWindowsCert((const char *) ((String *) argv[0])->buffer);
}

#endif

bool SSLInit(Module *self) {
#define AddIntConstant(alias, value)                    \
    if(!ModuleAddIntConstant(self, #alias, (int)value)) \
        return false

    AddIntConstant(PROTO_TLS, SSLProtocol::TLS);
    AddIntConstant(PROTO_TLS_CLIENT, SSLProtocol::TLS_CLIENT);
    AddIntConstant(PROTO_TLS_SERVER, SSLProtocol::TLS_SERVER);

    AddIntConstant(VFY_CERT_NONE, SSLVerify::CERT_NONE);
    AddIntConstant(VFY_CERT_OPTIONAL, SSLVerify::CERT_OPTIONAL);
    AddIntConstant(VFY_CERT_REQUIRED, SSLVerify::CERT_REQUIRED);

    AddIntConstant(FILETYPE_ASN1, SSL_FILETYPE_ASN1);
    AddIntConstant(FILETYPE_PEM, SSL_FILETYPE_PEM);

    if (!TypeInit((TypeInfo *) type_sslcontext_, nullptr))
        return false;

    if (!TypeInit((TypeInfo *) type_sslsocket_, nullptr))
        return false;

    if (!TypeInit((TypeInfo *) type_ssl_error_, nullptr))
        return false;

    SSL_load_error_strings();
    SSL_library_init();
    return true;

#undef AddIntConstant
}

const PropertyBulk ssl_bulk[] = {
        MODULE_EXPORT_TYPE(type_sslcontext_),
        MODULE_EXPORT_TYPE(type_ssl_error_),
#ifdef _ARGON_PLATFORM_WINDOWS
        MODULE_EXPORT_FUNCTION(ssl_enumcerts_windows_),
#endif
        MODULE_EXPORT_SENTINEL
};

const ModuleInit module_ssl = {
        "_ssl",
        "This module is a wrapper around OpenSSL library. If you are looking "
        "for SSL features, you should import ssl, not _ssl!",
        ssl_bulk,
        SSLInit,
        nullptr
};
const argon::object::ModuleInit *argon::module::module_ssl_ = &module_ssl;
