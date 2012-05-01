#ifndef VCF_H
#define VCF_H

#include <stdint.h>
#include <limits.h>
#include <assert.h>

#ifndef KSTRING_T
#define KSTRING_T kstring_t
typedef struct __kstring_t {
	size_t l, m;
	char *s;
} kstring_t;
#endif

#ifndef kroundup32
#define kroundup32(x) (--(x), (x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16, ++(x))
#endif

/*******************
 * VCF file struct *
 *******************/

typedef struct {
	uint32_t is_bin:1, is_write:1, dummy:30;
	void *buf; // a string buffer, only for VCF
	void *fp;  // file pointer; actual type depending on is_bin and is_write
} vcfFile;

/*****************
 * Header struct *
 *****************/

#define VCF_DT_FLT  0
#define VCF_DT_INFO 1
#define VCF_DT_FMT  2
#define VCF_DT_CTG  3

#define VCF_TP_FLAG 0
#define VCF_TP_INT  1
#define VCF_TP_REAL 2
#define VCF_TP_STR  3

#define VCF_VTP_FIXED 0
#define VCF_VTP_VAR   1
#define VCF_VTP_A     2
#define VCF_VTP_G     3

typedef struct {
	uint32_t info[3]; // Number:20, var:4, Type:4, ColType:4
	int kid, rid, sid, rlen; // key-ID, ref-ID, sample-ID, ref-length
} vcf_keyinfo_t;

typedef struct {
	const char *key;
	const vcf_keyinfo_t *info;
} vcf_keypair_t;

typedef struct {
	int32_t n_ref, n_sample, n_key, l_text;
	vcf_keypair_t *key;
	int *r2k, *s2k;
	char *text;
	void *dict;
} vcf_hdr_t;

extern uint8_t vcf_type_size[];

#define vcf_hdr_n_val(x, _n_alt) (((x)>>8&0xf) == VCF_VTP_FIXED? (x)>>12 : ((x)>>8&0xf) == VCF_VTP_A? (_n_alt) : ((x)>>8&0xf) == VCF_VTP_G? -2 : -1)

/**************
 * VCF record *
 **************/

#define VCF_RT_INT8		1
#define VCF_RT_INT16	2
#define VCF_RT_INT32	3
#define VCF_RT_INT64	4
#define VCF_RT_FLOAT	5
#define VCF_RT_BOOL		8
#define VCF_RT_CSTR		9
#define VCF_RT_UINT8	13

typedef struct {
	uint32_t key:28, type:3, is_vec:1;
	union {
		int32_t x;
		float f;
	} x;
} vcf_info_t;

typedef struct {
	int32_t rid; // CHROM
	int32_t pos; // POS
	float qual;  // QUAL
	uint16_t n_alt, n_fmt;
	int l_str, m_str;
	int o_ref, o_alt, o_flt, o_info, o_fmt, o_gt; // offsets in str
	int *alt, *flt;
	char *str;
} vcf1_t;

/*******************
 * Typed value I/O *
 *******************/

static inline void vcf_enc_size(kstring_t *s, int size, int type)
{
	if (size >= 15) {
		assert(size < 256); // not implemented yet
		kputc(15<<4|type, s);
		kputc(1<<4|VCF_RT_UINT8, s);
		kputc(size, s);
	} else kputc(size<<4|type, s);
}

static inline int vcf_enc_inttype(long x)
{
	if (x <= INT8_MAX && x > INT8_MIN) return VCF_RT_INT8;
	if (x <= INT16_MAX && x > INT16_MIN) return VCF_RT_INT16;
	return VCF_RT_INT32;
}

static inline void vcf_enc_int1(kstring_t *s, int32_t x)
{
	if (x == INT32_MIN) {
		vcf_enc_size(s, 1, VCF_RT_INT8);
		kputc(INT8_MIN, s);
	} else if (x <= INT8_MAX && x > INT8_MIN) {
		vcf_enc_size(s, 1, VCF_RT_INT8);
		kputc(x, s);
	} else if (x <= INT16_MAX && x > INT16_MIN) {
		int16_t z = x;
		vcf_enc_size(s, 1, VCF_RT_INT16);
		kputsn((char*)&z, 2, s);
	} else {
		int32_t z = x;
		vcf_enc_size(s, 1, VCF_RT_INT32);
		kputsn((char*)&z, 4, s);
	}
}

static inline int32_t vcf_dec_int1(const uint8_t *buf)
{
	assert(*buf>>4 == 1 && (*buf&0xf) <= 4);
	if ((*buf&0xf) == VCF_RT_INT8) return buf[1];
	else if ((*buf&0xf) == VCF_RT_INT16) return *(int16_t*)(buf + 1);
	else return *(int32_t*)(buf + 1);
}

static inline int64_t vcf_dec_size(const uint8_t *buf) // return value: lower 4 bits: offset of the first value
{
	if (*buf>>7 == 0) return (long)(*buf>>4)<<4 | 1;
	return (int64_t)vcf_dec_int1(buf + 1) << 4 | (1 + vcf_type_size[buf[1]&0xf]);
}

/*******
 * API *
 *******/

#ifdef __cplusplus
extern "C" {
#endif

	vcfFile *vcf_open(const char *fn, const char *mode, const char *fn_ref);
	void vcf_close(vcfFile *fp);
	vcf_hdr_t *vcf_hdr_read(vcfFile *fp);
	void vcf_hdr_destroy(vcf_hdr_t *h);

	vcf1_t *vcf_init1(void);
	void vcf_destroy1(vcf1_t *v);
	int vcf_read1(vcfFile *fp, const vcf_hdr_t *h, vcf1_t *v);
	int vcf_format1(const vcf_hdr_t *h, const vcf1_t *v, kstring_t *s);
	void vcf_write1(vcfFile *fp, const vcf1_t *v);

#ifdef __cplusplus
}
#endif

#endif