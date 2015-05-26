/*
 * Feed all manner of horrendous PDUs into the decode routines
 * to see what level of havoc can be caused.  Builds up custom
 * PDU structs (not possible using the __pmSend* routines) and
 * calls the __pmDecode* routines.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/trace.h>
#include <pcp/trace_dev.h>

static void
decode_attr(const char *name)
{
    char		*value;
    int			sts, code, length;
    struct attr {
	__pmPDUHdr	hdr;
	int		code;
	char		value[0];
    } *attr;

    attr = (struct attr *)malloc(sizeof(*attr));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(attr, 0, sizeof(*attr));
    sts = __pmDecodeAttr((__pmPDU *)attr, &code, &value, &length);
    fprintf(stderr, "  __pmDecodeAttr: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking negative length\n", name);
    memset(attr, 0, sizeof(*attr));
    attr->hdr.len = -512;
    attr->hdr.type = PDU_AUTH;
    attr->code = htonl(PCP_ATTR_USERID);
    sts = __pmDecodeAttr((__pmPDU *)attr, &code, &value, &length);
    fprintf(stderr, "  __pmDecodeAttr: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking empty value\n", name);
    memset(attr, 0, sizeof(*attr));
    attr->hdr.len = sizeof(*attr);
    attr->hdr.type = PDU_AUTH;
    attr->code = htonl(PCP_ATTR_USERID);
    sts = __pmDecodeAttr((__pmPDU *)attr, &code, &value, &length);
    fprintf(stderr, "  __pmDecodeAttr: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking access beyond limit\n", name);
    memset(attr, 0, sizeof(*attr));
    attr->hdr.len = INT_MAX;
    attr->hdr.type = PDU_AUTH;
    attr->code = htonl(PCP_ATTR_USERID);
    sts = __pmDecodeAttr((__pmPDU *)attr, &code, &value, &length);
    fprintf(stderr, "  __pmDecodeAttr: sts = %d (%s)\n", sts, pmErrStr(sts));

    free(attr);
}

static void
decode_creds(const char *name)
{
    __pmCred		*outcreds;
    int			sts, count, sender;
    struct creds {
	__pmPDUHdr	hdr;
	int		numcreds;
	__pmCred	credslist[0];
    } *creds;

    creds = (struct creds *)malloc(sizeof(*creds));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(creds, 0, sizeof(*creds));
    sts = __pmDecodeCreds((__pmPDU *)creds, &sender, &count, &outcreds);
    fprintf(stderr, "  __pmDecodeCreds: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(outcreds); }

    fprintf(stderr, "[%s] checking large numcred field\n", name);
    memset(creds, 0, sizeof(*creds));
    creds->hdr.len = sizeof(*creds);
    creds->hdr.type = PDU_CREDS;
    creds->numcreds = htonl(INT_MAX - 1);
    sts = __pmDecodeCreds((__pmPDU *)creds, &sender, &count, &outcreds);
    fprintf(stderr, "  __pmDecodeCreds: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(outcreds); }

    fprintf(stderr, "[%s] checking negative numcred field\n", name);
    memset(creds, 0, sizeof(*creds));
    creds->hdr.len = sizeof(*creds);
    creds->hdr.type = PDU_CREDS;
    creds->numcreds = htonl(-2);
    sts = __pmDecodeCreds((__pmPDU *)creds, &sender, &count, &outcreds);
    fprintf(stderr, "  __pmDecodeCreds: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(outcreds); }

    fprintf(stderr, "[%s] checking access beyond buffer\n", name);
    memset(creds, 0, sizeof(*creds));
    creds->hdr.len = sizeof(*creds);
    creds->hdr.type = PDU_CREDS;
    creds->numcreds = htonl(2);
    sts = __pmDecodeCreds((__pmPDU *)creds, &sender, &count, &outcreds);
    fprintf(stderr, "  __pmDecodeCreds: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(outcreds); }

    free(creds);
}

static void
decode_error(const char *name)
{
    int			sts, code, data;
    struct error {
	__pmPDUHdr	hdr;
    } *error;
    struct xerror {
	__pmPDUHdr	hdr;
	int		code[1];
    } *xerror;

    error = (struct error *)malloc(sizeof(*error));
    xerror = (struct xerror *)malloc(sizeof(*xerror));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(error, 0, sizeof(*error));
    sts = __pmDecodeError((__pmPDU *)error, &code);
    fprintf(stderr, "  __pmDecodeError: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking all-zeroes extended structure\n", name);
    memset(xerror, 0, sizeof(*xerror));
    sts = __pmDecodeXtendError((__pmPDU *)xerror, &code, &data);
    fprintf(stderr, "  __pmDecodeXtendError: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking access beyond buffer\n", name);
    memset(error, 0, sizeof(*error));
    error->hdr.len = sizeof(*error);
    error->hdr.type = PDU_ERROR;
    sts = __pmDecodeError((__pmPDU *)error, &code);
    fprintf(stderr, "  __pmDecodeError: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking access beyond extended buffer\n", name);
    memset(xerror, 0, sizeof(*xerror));
    xerror->hdr.len = sizeof(*xerror);
    xerror->hdr.type = PDU_ERROR;
    sts = __pmDecodeXtendError((__pmPDU *)xerror, &code, &data);
    fprintf(stderr, "  __pmDecodeXtendError: sts = %d (%s)\n", sts, pmErrStr(sts));

    free(xerror);
    free(error);
}

static void
decode_profile(const char *name)
{
    __pmProfile		*outprofs;
    int			sts, ctxnum;
    struct profile {
	__pmPDUHdr	hdr;
	int		unused[2];
	int		numprof;
	int		padding;
    } *profile;
    struct instprof {
	struct profile	profile;
	pmInDom		indom;
	int		state;
	int		numinst;
	int		padding;
    } *instprof;

    profile = (struct profile *)malloc(sizeof(*profile));
    instprof = (struct instprof *)malloc(sizeof(*instprof));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(profile, 0, sizeof(*profile));
    sts = __pmDecodeProfile((__pmPDU *)profile, &ctxnum, &outprofs);
    fprintf(stderr, "  __pmDecodeProfile: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(outprofs); }

    fprintf(stderr, "[%s] checking large numprof field\n", name);
    memset(profile, 0, sizeof(*profile));
    profile->hdr.len = sizeof(*profile);
    profile->hdr.type = PDU_PROFILE;
    profile->numprof = htonl(INT_MAX - 42);
    sts = __pmDecodeProfile((__pmPDU *)profile, &ctxnum, &outprofs);
    fprintf(stderr, "  __pmDecodeProfile: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(outprofs); }

    fprintf(stderr, "[%s] checking negative numprof field\n", name);
    memset(profile, 0, sizeof(*profile));
    profile->hdr.len = sizeof(*profile);
    profile->hdr.type = PDU_PROFILE;
    profile->numprof = htonl(-2);
    sts = __pmDecodeProfile((__pmPDU *)profile, &ctxnum, &outprofs);
    fprintf(stderr, "  __pmDecodeProfile: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(outprofs); }

    fprintf(stderr, "[%s] checking access beyond basic buffer\n", name);
    memset(profile, 0, sizeof(*profile));
    profile->hdr.len = sizeof(*profile);
    profile->hdr.type = PDU_PROFILE;
    profile->numprof = htonl(2);
    sts = __pmDecodeProfile((__pmPDU *)profile, &ctxnum, &outprofs);
    fprintf(stderr, "  __pmDecodeProfile: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(outprofs); }

    fprintf(stderr, "[%s] checking large numinst field\n", name);
    memset(instprof, 0, sizeof(*instprof));
    instprof->profile.hdr.len = sizeof(*instprof);
    instprof->profile.hdr.type = PDU_PROFILE;
    instprof->profile.numprof = htonl(1);
    instprof->numinst = htonl(INT_MAX - 3);
    sts = __pmDecodeProfile((__pmPDU *)instprof, &ctxnum, &outprofs);
    fprintf(stderr, "  __pmDecodeProfile: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(outprofs); }

    fprintf(stderr, "[%s] checking negative numinst field\n", name);
    memset(instprof, 0, sizeof(*instprof));
    instprof->profile.hdr.len = sizeof(*instprof);
    instprof->profile.hdr.type = PDU_PROFILE;
    instprof->profile.numprof = htonl(1);
    instprof->numinst = htonl(-3);
    sts = __pmDecodeProfile((__pmPDU *)instprof, &ctxnum, &outprofs);
    fprintf(stderr, "  __pmDecodeProfile: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(outprofs); }

    fprintf(stderr, "[%s] checking access beyond extended buffer\n", name);
    memset(instprof, 0, sizeof(*instprof));
    instprof->profile.hdr.len = sizeof(*instprof);
    instprof->profile.hdr.type = PDU_PROFILE;
    instprof->profile.numprof = htonl(1);
    instprof->numinst = htonl(2);
    sts = __pmDecodeProfile((__pmPDU *)instprof, &ctxnum, &outprofs);
    fprintf(stderr, "  __pmDecodeProfile: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(outprofs); }

    free(instprof);
    free(profile);
}

static void
decode_fetch(const char *name)
{
    __pmTimeval		when;
    pmID		*pmidlist;
    int			sts, ctx, count;
    struct fetch {
	__pmPDUHdr	hdr;
	int		ctxnum;
	__pmTimeval	when;
	int		numpmid;
	pmID		pmidlist[0];
    } *fetch;

    fetch = (struct fetch *)malloc(sizeof(*fetch));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(fetch, 0, sizeof(*fetch));
    sts = __pmDecodeFetch((__pmPDU *)fetch, &ctx, &when, &count, &pmidlist);
    fprintf(stderr, "  __pmDecodeFetch: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(pmidlist); }

    fprintf(stderr, "[%s] checking large numpmid field\n", name);
    memset(fetch, 0, sizeof(*fetch));
    fetch->hdr.len = sizeof(*fetch);
    fetch->hdr.type = PDU_FETCH;
    fetch->numpmid = htonl(INT_MAX - 1);
    sts = __pmDecodeFetch((__pmPDU *)fetch, &ctx, &when, &count, &pmidlist);
    fprintf(stderr, "  __pmDecodeFetch: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(pmidlist); }

    fprintf(stderr, "[%s] checking negative numpmid field\n", name);
    memset(fetch, 0, sizeof(*fetch));
    fetch->hdr.len = sizeof(*fetch);
    fetch->hdr.type = PDU_FETCH;
    fetch->numpmid = htonl(-2);
    sts = __pmDecodeFetch((__pmPDU *)fetch, &ctx, &when, &count, &pmidlist);
    fprintf(stderr, "  __pmDecodeFetch: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(pmidlist); }

    fprintf(stderr, "[%s] checking access beyond buffer\n", name);
    memset(fetch, 0, sizeof(*fetch));
    fetch->hdr.len = sizeof(*fetch);
    fetch->hdr.type = PDU_FETCH;
    fetch->numpmid = htonl(2);
    sts = __pmDecodeFetch((__pmPDU *)fetch, &ctx, &when, &count, &pmidlist);
    fprintf(stderr, "  __pmDecodeFetch: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(pmidlist); }

    free(fetch);
}

static void
decode_desc_req(const char *name)
{
    pmID		pmid;
    int			sts;
    struct desc_req {
	__pmPDUHdr	hdr;
    } *desc_req;

    desc_req = (struct desc_req *)malloc(sizeof(*desc_req));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(desc_req, 0, sizeof(*desc_req));
    sts = __pmDecodeDescReq((__pmPDU *)desc_req, &pmid);
    fprintf(stderr, "  __pmDecodeDescReq: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking access beyond buffer\n", name);
    memset(desc_req, 0, sizeof(*desc_req));
    desc_req->hdr.len = sizeof(*desc_req);
    desc_req->hdr.type = PDU_DESC_REQ;
    sts = __pmDecodeDescReq((__pmPDU *)desc_req, &pmid);
    fprintf(stderr, "  __pmDecodeDescReq: sts = %d (%s)\n", sts, pmErrStr(sts));

    free(desc_req);
}

static void
decode_desc(const char *name)
{
    pmDesc		pmdesc;
    int			sts;
    struct desc {
	__pmPDUHdr	hdr;
    } *desc;

    desc = (struct desc *)malloc(sizeof(*desc));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(desc, 0, sizeof(*desc));
    sts = __pmDecodeDesc((__pmPDU *)desc, &pmdesc);
    fprintf(stderr, "  __pmDecodeDesc: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking access beyond buffer\n", name);
    memset(desc, 0, sizeof(*desc));
    desc->hdr.len = sizeof(*desc);
    desc->hdr.type = PDU_DESC;
    sts = __pmDecodeDesc((__pmPDU *)desc, &pmdesc);
    fprintf(stderr, "  __pmDecodeDesc: sts = %d (%s)\n", sts, pmErrStr(sts));

    free(desc);
}

static void
decode_instance_req(const char *name)
{
    __pmTimeval		when;
    pmInDom		indom;
    int			inst, sts;
    char		*resname;
    struct instance_req {
	__pmPDUHdr	hdr;
	pmInDom		indom;
	__pmTimeval	when;
	int		inst;
	int		namelen;
	char		name[0];
    } *instance_req, *xinstance_req;

    instance_req = (struct instance_req *)malloc(sizeof(*instance_req));
    xinstance_req = (struct instance_req *)malloc(sizeof(*xinstance_req)+16);

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(instance_req, 0, sizeof(*instance_req));
    sts = __pmDecodeInstanceReq((__pmPDU *)instance_req, &when, &indom, &inst, &resname);
    fprintf(stderr, "  __pmDecodeInstanceReq: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(resname); }

    fprintf(stderr, "[%s] checking large namelen field\n", name);
    memset(instance_req, 0, sizeof(*instance_req));
    instance_req->hdr.len = sizeof(*instance_req);
    instance_req->hdr.type = PDU_INSTANCE_REQ;
    instance_req->namelen = htonl(INT_MAX - 1);
    sts = __pmDecodeInstanceReq((__pmPDU *)instance_req, &when, &indom, &inst, &resname);
    fprintf(stderr, "  __pmDecodeInstanceReq: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(resname); }

    fprintf(stderr, "[%s] checking negative namelen field\n", name);
    memset(instance_req, 0, sizeof(*instance_req));
    instance_req->hdr.len = sizeof(*instance_req);
    instance_req->hdr.type = PDU_INSTANCE_REQ;
    instance_req->namelen = htonl(-2);
    sts = __pmDecodeInstanceReq((__pmPDU *)instance_req, &when, &indom, &inst, &resname);
    fprintf(stderr, "  __pmDecodeInstanceReq: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(resname); }

    fprintf(stderr, "[%s] checking access beyond basic buffer\n", name);
    memset(instance_req, 0, sizeof(*instance_req));
    instance_req->hdr.len = sizeof(*instance_req);
    instance_req->hdr.type = PDU_INSTANCE_REQ;
    instance_req->namelen = htonl(1);
    sts = __pmDecodeInstanceReq((__pmPDU *)instance_req, &when, &indom, &inst, &resname);
    fprintf(stderr, "  __pmDecodeInstanceReq: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(resname); }

    fprintf(stderr, "[%s] checking access beyond extended buffer\n", name);
    memset(xinstance_req, 0, sizeof(*xinstance_req) + 16);
    xinstance_req->hdr.len = sizeof(*xinstance_req) + 16;
    xinstance_req->hdr.type = PDU_INSTANCE_REQ;
    xinstance_req->namelen = htonl(32);
    sts = __pmDecodeInstanceReq((__pmPDU *)xinstance_req, &when, &indom, &inst, &resname);
    fprintf(stderr, "  __pmDecodeInstanceReq: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) {free(resname); }

    free(xinstance_req);
    free(instance_req);
}

static void
decode_instance(const char *name)
{
    __pmInResult	*inresult;
    int			sts;
    struct instance {
	__pmPDUHdr	hdr;
	pmInDom		indom;
	int		numinst;
	__pmPDU		rest[0];
    } *instance;
    struct instlist {
	struct instance	instance;
	int		inst;
	int		namelen;
	char		name[0];
    } *instlist;

    instance = (struct instance *)malloc(sizeof(*instance));
    instlist = (struct instlist *)malloc(sizeof(*instlist));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(instance, 0, sizeof(*instance));
    sts = __pmDecodeInstance((__pmPDU *)instance, &inresult);
    fprintf(stderr, "  __pmDecodeInstance: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) __pmFreeInResult(inresult);

    fprintf(stderr, "[%s] checking large numinst field\n", name);
    memset(instance, 0, sizeof(*instance));
    instance->hdr.len = sizeof(*instance);
    instance->hdr.type = PDU_INSTANCE;
    instance->numinst = htonl(INT_MAX - 42);
    sts = __pmDecodeInstance((__pmPDU *)instance, &inresult);
    fprintf(stderr, "  __pmDecodeInstance: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) __pmFreeInResult(inresult);

    fprintf(stderr, "[%s] checking negative numinst field\n", name);
    memset(instance, 0, sizeof(*instance));
    instance->hdr.len = sizeof(*instance);
    instance->hdr.type = PDU_INSTANCE;
    instance->numinst = htonl(-2);
    sts = __pmDecodeInstance((__pmPDU *)instance, &inresult);
    fprintf(stderr, "  __pmDecodeInstance: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) __pmFreeInResult(inresult);

    fprintf(stderr, "[%s] checking access beyond basic buffer\n", name);
    memset(instance, 0, sizeof(*instance));
    instance->hdr.len = sizeof(*instance);
    instance->hdr.type = PDU_INSTANCE;
    instance->numinst = htonl(1);
    sts = __pmDecodeInstance((__pmPDU *)instance, &inresult);
    fprintf(stderr, "  __pmDecodeInstance: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) __pmFreeInResult(inresult);

    fprintf(stderr, "[%s] checking large namelen field\n", name);
    memset(instlist, 0, sizeof(*instlist));
    instlist->instance.hdr.len = sizeof(*instlist);
    instlist->instance.hdr.type = PDU_INSTANCE;
    instlist->instance.numinst = htonl(1);
    instlist->namelen = htonl(INT_MAX - 5);
    sts = __pmDecodeInstance((__pmPDU *)instlist, &inresult);
    fprintf(stderr, "  __pmDecodeInstance: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) __pmFreeInResult(inresult);

    fprintf(stderr, "[%s] checking negative namelen field\n", name);
    memset(instlist, 0, sizeof(*instlist));
    instlist->instance.hdr.len = sizeof(*instlist);
    instlist->instance.hdr.type = PDU_INSTANCE;
    instlist->instance.numinst = htonl(1);
    instlist->namelen = htonl(-2);
    sts = __pmDecodeInstance((__pmPDU *)instlist, &inresult);
    fprintf(stderr, "  __pmDecodeInstance: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) __pmFreeInResult(inresult);

    fprintf(stderr, "[%s] checking access beyond extended buffer\n", name);
    memset(instlist, 0, sizeof(*instlist));
    instlist->instance.hdr.len = sizeof(*instlist);
    instlist->instance.hdr.type = PDU_INSTANCE;
    instlist->instance.numinst = htonl(1);
    instlist->namelen = htonl(32);
    sts = __pmDecodeInstance((__pmPDU *)instlist, &inresult);
    fprintf(stderr, "  __pmDecodeInstance: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) __pmFreeInResult(inresult);

    free(instlist);
    free(instance);
}

static void
decode_pmns_ids(const char *name)
{
    pmID		idarray[10];
    int			idsts, sts;
    struct idlist {
	__pmPDUHdr	hdr;
	int		sts;
	int		numids;
	pmID		idlist[0];
    } *idlist;

    idlist = (struct idlist *)malloc(sizeof(*idlist));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(idlist, 0, sizeof(*idlist));
    sts = __pmDecodeIDList((__pmPDU *)idlist, 10, idarray, &idsts);
    fprintf(stderr, "  __pmDecodeIDList: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking large numids field\n", name);
    memset(idlist, 0, sizeof(*idlist));
    idlist->hdr.len = sizeof(*idlist);
    idlist->hdr.type = PDU_PMNS_IDS;
    idlist->numids = htonl(INT_MAX - 1);
    sts = __pmDecodeIDList((__pmPDU *)idlist, 10, idarray, &idsts);
    fprintf(stderr, "  __pmDecodeIDList: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking negative numids field\n", name);
    memset(idlist, 0, sizeof(*idlist));
    idlist->hdr.len = sizeof(*idlist);
    idlist->hdr.type = PDU_PMNS_IDS;
    idlist->numids = htonl(-2);
    sts = __pmDecodeIDList((__pmPDU *)idlist, 10, idarray, &idsts);
    fprintf(stderr, "  __pmDecodeIDList: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking access beyond buffer\n", name);
    memset(idlist, 0, sizeof(*idlist));
    idlist->hdr.len = sizeof(*idlist);
    idlist->hdr.type = PDU_PMNS_IDS;
    idlist->numids = htonl(2);
    sts = __pmDecodeIDList((__pmPDU *)idlist, 10, idarray, &idsts);
    fprintf(stderr, "  __pmDecodeIDList: sts = %d (%s)\n", sts, pmErrStr(sts));

    free(idlist);
}

static void
decode_pmns_names(const char *name)
{
    int			sts, numnames, *status;
    char		**names;
    struct namelist {
	__pmPDUHdr	hdr;
	int		nstrbytes;
	int		numstatus;
	int		numnames;
	pmID		names[0];
    } *namelist;
    struct namestatus {
	struct namelist	namelist;
	int		status;
	int		namelen;
	char		name[0];
    } *namestatus;

    namelist = (struct namelist *)malloc(sizeof(*namelist));
    namestatus = (struct namestatus *)malloc(sizeof(*namestatus));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(namelist, 0, sizeof(*namelist));
    sts = __pmDecodeNameList((__pmPDU *)namelist, &numnames, &names, &status);
    fprintf(stderr, "  __pmDecodeNameList: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(status); free(names); }

    fprintf(stderr, "[%s] checking large numnames field\n", name);
    memset(namelist, 0, sizeof(*namelist));
    namelist->hdr.len = sizeof(*namelist);
    namelist->hdr.type = PDU_PMNS_NAMES;
    namelist->numnames = htonl(INT_MAX - 42);
    sts = __pmDecodeNameList((__pmPDU *)namelist, &numnames, &names, &status);
    fprintf(stderr, "  __pmDecodeNameList: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(status); free(names); }

    fprintf(stderr, "[%s] checking negative numnames field\n", name);
    memset(namelist, 0, sizeof(*namelist));
    namelist->hdr.len = sizeof(*namelist);
    namelist->hdr.type = PDU_PMNS_NAMES;
    namelist->numnames = htonl(-42);
    sts = __pmDecodeNameList((__pmPDU *)namelist, &numnames, &names, &status);
    fprintf(stderr, "  __pmDecodeNameList: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(status); free(names); }

    fprintf(stderr, "[%s] checking large nstrbytes field\n", name);
    memset(namelist, 0, sizeof(*namelist));
    namelist->hdr.len = sizeof(*namelist);
    namelist->hdr.type = PDU_PMNS_NAMES;
    namelist->numnames = htonl(42);
    namelist->nstrbytes = htonl(INT_MAX - 42);
    sts = __pmDecodeNameList((__pmPDU *)namelist, &numnames, &names, &status);
    fprintf(stderr, "  __pmDecodeNameList: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(status); free(names); }

    fprintf(stderr, "[%s] checking negative nstrbytes field\n", name);
    memset(namelist, 0, sizeof(*namelist));
    namelist->hdr.len = sizeof(*namelist);
    namelist->hdr.type = PDU_PMNS_NAMES;
    namelist->numnames = htonl(42);
    namelist->nstrbytes = htonl(-42);
    sts = __pmDecodeNameList((__pmPDU *)namelist, &numnames, &names, &status);
    fprintf(stderr, "  __pmDecodeNameList: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(status); free(names); }

    fprintf(stderr, "[%s] checking access beyond basic buffer\n", name);
    memset(namelist, 0, sizeof(*namelist));
    namelist->hdr.len = sizeof(*namelist);
    namelist->hdr.type = PDU_PMNS_NAMES;
    namelist->numnames = htonl(1);
    sts = __pmDecodeNameList((__pmPDU *)namelist, &numnames, &names, &status);
    fprintf(stderr, "  __pmDecodeNameList: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(status); free(names); }

    fprintf(stderr, "[%s] checking large namelen field\n", name);
    memset(namestatus, 0, sizeof(*namestatus));
    namestatus->namelist.hdr.len = sizeof(*namestatus);
    namestatus->namelist.hdr.type = PDU_PMNS_NAMES;
    namestatus->namelist.numnames = htonl(1);
    namestatus->namelen = htonl(INT_MAX - 5);
    sts = __pmDecodeNameList((__pmPDU *)namestatus, &numnames, &names, &status);
    fprintf(stderr, "  __pmDecodeNameList: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(status); free(names); }

    fprintf(stderr, "[%s] checking negative namelen field\n", name);
    memset(namestatus, 0, sizeof(*namestatus));
    namestatus->namelist.hdr.len = sizeof(*namestatus);
    namestatus->namelist.hdr.type = PDU_PMNS_NAMES;
    namestatus->namelist.numnames = htonl(1);
    namestatus->namelen = htonl(-2);
    sts = __pmDecodeNameList((__pmPDU *)namestatus, &numnames, &names, &status);
    fprintf(stderr, "  __pmDecodeNameList: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(status); free(names); }

    fprintf(stderr, "[%s] checking access beyond extended buffer\n", name);
    memset(namestatus, 0, sizeof(*namestatus));
    namestatus->namelist.hdr.len = sizeof(*namestatus);
    namestatus->namelist.hdr.type = PDU_PMNS_NAMES;
    namestatus->namelist.numnames = htonl(1);
    namestatus->namelen = htonl(32);
    sts = __pmDecodeNameList((__pmPDU *)namestatus, &numnames, &names, &status);
    fprintf(stderr, "  __pmDecodeNameList: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(status); free(names); }

    fprintf(stderr, "[%s] checking large namelen field (+statuslist)\n", name);
    memset(namestatus, 0, sizeof(*namestatus));
    namestatus->namelist.hdr.len = sizeof(*namestatus);
    namestatus->namelist.hdr.type = PDU_PMNS_NAMES;
    namestatus->namelist.numnames = namestatus->namelist.numstatus = htonl(1);
    namestatus->namelen = htonl(INT_MAX - 5);
    sts = __pmDecodeNameList((__pmPDU *)namestatus, &numnames, &names, &status);
    fprintf(stderr, "  __pmDecodeNameList: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(status); free(names); }

    fprintf(stderr, "[%s] checking negative namelen field (+statuslist)\n", name);
    memset(namestatus, 0, sizeof(*namestatus));
    namestatus->namelist.hdr.len = sizeof(*namestatus);
    namestatus->namelist.hdr.type = PDU_PMNS_NAMES;
    namestatus->namelist.numnames = namestatus->namelist.numstatus = htonl(1);
    namestatus->namelen = htonl(-2);
    sts = __pmDecodeNameList((__pmPDU *)namestatus, &numnames, &names, &status);
    fprintf(stderr, "  __pmDecodeNameList: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(status); free(names); }

    fprintf(stderr, "[%s] checking access beyond extended buffer (+statuslist)\n", name);
    memset(namestatus, 0, sizeof(*namestatus));
    namestatus->namelist.hdr.len = sizeof(*namestatus);
    namestatus->namelist.hdr.type = PDU_PMNS_NAMES;
    namestatus->namelist.numnames = namestatus->namelist.numstatus = htonl(1);
    namestatus->namelen = htonl(32);
    sts = __pmDecodeNameList((__pmPDU *)namestatus, &numnames, &names, &status);
    fprintf(stderr, "  __pmDecodeNameList: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(status); free(names); }

    free(namestatus);
    free(namelist);
}

/* Wraps __pmDecodeChildReq and __pmDecodeTraversePMNSReq interfaces */
static void
decode_name_request(const char *name, const char *caller, int pdutype)
{
    char		*resnames;
    int			sts, restype;
    struct name_req {
	__pmPDUHdr	hdr;
	int		subtype;
	int		namelen;
	char		name[0];
    } *name_req, *xname_req;

    name_req = (struct name_req *)malloc(sizeof(*name_req));
    xname_req = (struct name_req *)malloc(sizeof(*name_req) + 16);

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(name_req, 0, sizeof(*name_req));
    sts = (pdutype == PDU_PMNS_TRAVERSE) ?
	__pmDecodeTraversePMNSReq((__pmPDU *)name_req, &resnames) :
	__pmDecodeChildReq((__pmPDU *)name_req, &resnames, &restype);
    fprintf(stderr, "  __pmDecode%sReq: sts = %d (%s)\n", caller, sts, pmErrStr(sts));
    if (sts >= 0) { free(resnames); }

    fprintf(stderr, "[%s] checking large namelen field\n", name);
    memset(name_req, 0, sizeof(*name_req));
    name_req->hdr.len = sizeof(*name_req);
    name_req->hdr.type = pdutype;
    name_req->namelen = htonl(INT_MAX - 1);
    sts = (pdutype == PDU_PMNS_TRAVERSE) ?
	__pmDecodeTraversePMNSReq((__pmPDU *)name_req, &resnames) :
	__pmDecodeChildReq((__pmPDU *)name_req, &resnames, &restype);
    fprintf(stderr, "  __pmDecode%sReq: sts = %d (%s)\n", caller, sts, pmErrStr(sts));
    if (sts >= 0) { free(resnames); }

    fprintf(stderr, "[%s] checking negative namelen field\n", name);
    memset(name_req, 0, sizeof(*name_req));
    name_req->hdr.len = sizeof(*name_req);
    name_req->hdr.type = pdutype;
    name_req->namelen = htonl(-2);
    sts = (pdutype == PDU_PMNS_TRAVERSE) ?
	__pmDecodeTraversePMNSReq((__pmPDU *)name_req, &resnames) :
	__pmDecodeChildReq((__pmPDU *)name_req, &resnames, &restype);
    fprintf(stderr, "  __pmDecode%sReq: sts = %d (%s)\n", caller, sts, pmErrStr(sts));
    if (sts >= 0) { free(resnames); }

    fprintf(stderr, "[%s] checking access beyond basic buffer\n", name);
    memset(name_req, 0, sizeof(*name_req));
    name_req->hdr.len = sizeof(*name_req);
    name_req->hdr.type = pdutype;
    name_req->namelen = htonl(1);
    sts = (pdutype == PDU_PMNS_TRAVERSE) ?
	__pmDecodeTraversePMNSReq((__pmPDU *)name_req, &resnames) :
	__pmDecodeChildReq((__pmPDU *)name_req, &resnames, &restype);
    fprintf(stderr, "  __pmDecode%sReq: sts = %d (%s)\n", caller, sts, pmErrStr(sts));
    if (sts >= 0) { free(resnames); }

    fprintf(stderr, "[%s] checking access beyond extended buffer\n", name);
    memset(xname_req, 0, sizeof(*xname_req));
    xname_req->hdr.len = sizeof(*xname_req) + 16;
    xname_req->hdr.type = pdutype;
    xname_req->namelen = htonl(32);
    sts = (pdutype == PDU_PMNS_TRAVERSE) ?
	__pmDecodeTraversePMNSReq((__pmPDU *)xname_req, &resnames) :
	__pmDecodeChildReq((__pmPDU *)xname_req, &resnames, &restype);
    fprintf(stderr, "  __pmDecode%sReq: sts = %d (%s)\n", caller, sts, pmErrStr(sts));
    if (sts >= 0) { free(resnames); }

    free(xname_req);
    free(name_req);
}

static void
decode_pmns_child(const char *name)
{
    decode_name_request(name, "Child", PDU_PMNS_CHILD);
}

static void
decode_pmns_traverse(const char *name)
{
    decode_name_request(name, "TraversePMNS", PDU_PMNS_TRAVERSE);
}

static void
decode_log_control(const char *name)
{
    pmResult	*result;
    int		sts, ctl, state, delta;
    struct log_ctl {
	__pmPDUHdr	hdr;
	int		el[3];
	int		numpmid;
	__pmPDU		data[0];
    } *log_ctl;
    struct logvlist {
	struct log_ctl	log_ctl;
	pmID		pmid;
	int		numval;
	__pmValue_PDU	vlist[0];
    } *logvlist;

    log_ctl = (struct log_ctl *)malloc(sizeof(*log_ctl));
    logvlist = (struct logvlist *)malloc(sizeof(*logvlist));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(log_ctl, 0, sizeof(*log_ctl));
    sts = __pmDecodeLogControl((__pmPDU *)log_ctl, &result, &ctl, &state, &delta);
    fprintf(stderr, "  __pmDecodeLogControl: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(result);

    fprintf(stderr, "[%s] checking large numpmid field\n", name);
    memset(log_ctl, 0, sizeof(*log_ctl));
    log_ctl->hdr.len = sizeof(*log_ctl);
    log_ctl->hdr.type = PDU_LOG_CONTROL;
    log_ctl->numpmid = htonl(INT_MAX - 42);
    sts = __pmDecodeLogControl((__pmPDU *)log_ctl, &result, &ctl, &state, &delta);
    fprintf(stderr, "  __pmDecodeLogControl: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(result);

    fprintf(stderr, "[%s] checking negative numpmid field\n", name);
    memset(log_ctl, 0, sizeof(*log_ctl));
    log_ctl->hdr.len = sizeof(*log_ctl);
    log_ctl->hdr.type = PDU_LOG_CONTROL;
    log_ctl->numpmid = htonl(-42);
    sts = __pmDecodeLogControl((__pmPDU *)log_ctl, &result, &ctl, &state, &delta);
    fprintf(stderr, "  __pmDecodeLogControl: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(result);

    fprintf(stderr, "[%s] checking access beyond basic buffer\n", name);
    memset(log_ctl, 0, sizeof(*log_ctl));
    log_ctl->hdr.len = sizeof(*log_ctl);
    log_ctl->hdr.type = PDU_LOG_CONTROL;
    log_ctl->numpmid = htonl(2);
    sts = __pmDecodeLogControl((__pmPDU *)log_ctl, &result, &ctl, &state, &delta);
    fprintf(stderr, "  __pmDecodeLogControl: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(result);

    fprintf(stderr, "[%s] checking large numval field\n", name);
    memset(logvlist, 0, sizeof(*logvlist));
    logvlist->log_ctl.hdr.len = sizeof(*logvlist);
    logvlist->log_ctl.hdr.type = PDU_LOG_CONTROL;
    logvlist->log_ctl.numpmid = htonl(1);
    logvlist->numval = htonl(INT_MAX - 3);
    sts = __pmDecodeLogControl((__pmPDU *)logvlist, &result, &ctl, &state, &delta);
    fprintf(stderr, "  __pmDecodeLogControl: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(result);

    fprintf(stderr, "[%s] checking access beyond extended buffer\n", name);
    memset(logvlist, 0, sizeof(*logvlist));
    logvlist->log_ctl.hdr.len = sizeof(*logvlist);
    logvlist->log_ctl.hdr.type = PDU_LOG_CONTROL;
    logvlist->log_ctl.numpmid = htonl(1);
    logvlist->numval = htonl(2);
    sts = __pmDecodeLogControl((__pmPDU *)logvlist, &result, &ctl, &state, &delta);
    fprintf(stderr, "  __pmDecodeLogControl: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(result);

    free(log_ctl);
    free(logvlist);
}

static void
decode_log_status(const char *name)
{
    __pmLoggerStatus	*log;
    int			sts;
    struct log_sts {
	__pmPDUHdr	hdr;
	int		pad;
	__pmLoggerStatus sts;
    } *log_sts;

    log_sts = (struct log_sts *)malloc(sizeof(*log_sts));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(log_sts, 0, sizeof(*log_sts));
    sts = __pmDecodeLogStatus((__pmPDU *)log_sts, &log);
    fprintf(stderr, "  __pmDecodeLogStatus: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking access beyond buffer\n", name);
    memset(log_sts, 0, sizeof(*log_sts));
    log_sts->hdr.len = sizeof(*log_sts) - 4;
    log_sts->hdr.type = PDU_LOG_STATUS;
    sts = __pmDecodeLogStatus((__pmPDU *)log_sts, &log);
    fprintf(stderr, "  __pmDecodeLogStatus: sts = %d (%s)\n", sts, pmErrStr(sts));

    free(log_sts);
}

static void
decode_log_request(const char *name)
{
    int			type, sts;
    struct log_req {
	__pmPDUHdr	hdr;
    } *log_req;

    log_req = (struct log_req *)malloc(sizeof(*log_req));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(log_req, 0, sizeof(*log_req));
    sts = __pmDecodeLogRequest((__pmPDU *)log_req, &type);
    fprintf(stderr, "  __pmDecodeLogRequest: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking access beyond buffer\n", name);
    memset(log_req, 0, sizeof(*log_req));
    log_req->hdr.len = sizeof(*log_req);
    log_req->hdr.type = PDU_LOG_REQUEST;
    sts = __pmDecodeLogRequest((__pmPDU *)log_req, &type);
    fprintf(stderr, "  __pmDecodeLogRequest: sts = %d (%s)\n", sts, pmErrStr(sts));

    free(log_req);
}

static void
decode_result(const char *name)
{
    int			sts;
    pmResult		*resp;
    struct result {
	__pmPDUHdr	hdr;
	__pmTimeval	stamp;
	int		numpmid;
	__pmPDU		data[0];
    } *result;
    struct resultlist {
	struct result	result;
	pmID		pmid;
	int		numval;
	int		valfmt;
	__pmValue_PDU	vlist[0];
    } *resultlist;
    struct resultdynlist {
	struct resultlist vlist;
	__pmValue_PDU	values;
	pmValueBlock	block[0];
    } *resultdynlist;

    result = (struct result *)malloc(sizeof(*result));
    resultlist = (struct resultlist *)malloc(sizeof(*resultlist));
    resultdynlist = (struct resultdynlist *)malloc(sizeof(*resultdynlist));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(result, 0, sizeof(*result));
    sts = __pmDecodeResult((__pmPDU *)result, &resp);
    fprintf(stderr, "  __pmDecodeResult: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(resp);

    fprintf(stderr, "[%s] checking large numpmid field\n", name);
    memset(result, 0, sizeof(*result));
    result->hdr.len = sizeof(*result);
    result->hdr.type = PDU_RESULT;
    result->numpmid = htonl(INT_MAX - 42);
    sts = __pmDecodeResult((__pmPDU *)result, &resp);
    fprintf(stderr, "  __pmDecodeResult: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(resp);

    fprintf(stderr, "[%s] checking negative numpmid field\n", name);
    memset(result, 0, sizeof(*result));
    result->hdr.len = sizeof(*result);
    result->hdr.type = PDU_RESULT;
    result->numpmid = htonl(-42);
    sts = __pmDecodeResult((__pmPDU *)result, &resp);
    fprintf(stderr, "  __pmDecodeResult: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(resp);

    fprintf(stderr, "[%s] checking access beyond basic buffer\n", name);
    memset(result, 0, sizeof(*result));
    result->hdr.len = sizeof(*result);
    result->hdr.type = PDU_RESULT;
    result->numpmid = htonl(4);
    sts = __pmDecodeResult((__pmPDU *)result, &resp);
    fprintf(stderr, "  __pmDecodeResult: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(resp);

    fprintf(stderr, "[%s] checking large numval field\n", name);
    memset(resultlist, 0, sizeof(*resultlist));
    resultlist->result.hdr.len = sizeof(*resultlist);
    resultlist->result.hdr.type = PDU_RESULT;
    resultlist->result.numpmid = htonl(1);
    resultlist->numval = htonl(INT_MAX - 3);
    sts = __pmDecodeResult((__pmPDU *)resultlist, &resp);
    fprintf(stderr, "  __pmDecodeResult: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(resp);

    fprintf(stderr, "[%s] checking negative numval field\n", name);
    memset(resultlist, 0, sizeof(*resultlist));
    resultlist->result.hdr.len = sizeof(*resultlist);
    resultlist->result.hdr.type = PDU_RESULT;
    resultlist->result.numpmid = htonl(1);
    resultlist->numval = htonl(-3);
    sts = __pmDecodeResult((__pmPDU *)resultlist, &resp);
    fprintf(stderr, "  __pmDecodeResult: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(resp);

    fprintf(stderr, "[%s] checking access beyond extended buffer\n", name);
    memset(resultlist, 0, sizeof(*resultlist));
    resultlist->result.hdr.len = sizeof(*resultlist);
    resultlist->result.hdr.type = PDU_LOG_CONTROL;
    resultlist->result.numpmid = htonl(1);
    resultlist->numval = htonl(2);
    sts = __pmDecodeResult((__pmPDU *)resultlist, &resp);
    fprintf(stderr, "  __pmDecodeResult: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(resp);

    fprintf(stderr, "[%s] checking insitu valfmt field\n", name);
    memset(resultlist, 0, sizeof(*resultlist));
    resultlist->result.hdr.len = sizeof(*resultlist);
    resultlist->result.hdr.type = PDU_RESULT;
    resultlist->result.numpmid = htonl(1);
    resultlist->valfmt = htonl(PM_VAL_INSITU);
    resultlist->numval = htonl(1);
    sts = __pmDecodeResult((__pmPDU *)resultlist, &resp);
    fprintf(stderr, "  __pmDecodeResult: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(resp);

    fprintf(stderr, "[%s] checking non-insitu valfmt field\n", name);
    memset(resultlist, 0, sizeof(*resultlist));
    resultlist->result.hdr.len = sizeof(*resultlist);
    resultlist->result.hdr.type = PDU_RESULT;
    resultlist->result.numpmid = htonl(1);
    resultlist->valfmt = htonl(PM_VAL_DPTR);
    resultlist->numval = htonl(1);
    sts = __pmDecodeResult((__pmPDU *)resultlist, &resp);
    fprintf(stderr, "  __pmDecodeResult: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(resp);

    fprintf(stderr, "[%s] checking access beyond non-insitu valfmt field\n", name);
    memset(resultdynlist, 0, sizeof(*resultdynlist));
    resultdynlist->vlist.result.hdr.len = sizeof(*resultdynlist);
    resultdynlist->vlist.result.hdr.type = PDU_RESULT;
    resultdynlist->vlist.result.numpmid = htonl(1);
    resultdynlist->vlist.valfmt = htonl(PM_VAL_DPTR);
    resultdynlist->vlist.numval = htonl(1);
    resultdynlist->values.value.lval = htonl(10);
    sts = __pmDecodeResult((__pmPDU *)resultdynlist, &resp);
    fprintf(stderr, "  __pmDecodeResult: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) pmFreeResult(resp);

    free(resultdynlist);
    free(resultlist);
    free(result);
}

static void
decode_text_req(const char *name)
{
    int			ident, type, sts;
    struct text_req {
	__pmPDUHdr	hdr;
	int		val[1];
    } *text_req;

    text_req = (struct text_req *)malloc(sizeof(*text_req));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(text_req, 0, sizeof(*text_req));
    sts = __pmDecodeTextReq((__pmPDU *)text_req, &ident, &type);
    fprintf(stderr, "  __pmDecodeTextReq: sts = %d (%s)\n", sts, pmErrStr(sts));

    fprintf(stderr, "[%s] checking access beyond buffer\n", name);
    memset(text_req, 0, sizeof(*text_req));
    text_req->hdr.len = sizeof(*text_req);
    text_req->hdr.type = PDU_TEXT_REQ;
    sts = __pmDecodeTextReq((__pmPDU *)text_req, &ident, &type);
    fprintf(stderr, "  __pmDecodeTextReq: sts = %d (%s)\n", sts, pmErrStr(sts));

    free(text_req);
}

static void
decode_text(const char *name)
{
    int			ident, sts;
    char		*buffer;
    struct text {
	__pmPDUHdr	hdr;
	int		ident;
	int		buflen;
	char		buffer[0];
    } *text;

    text = (struct text *)malloc(sizeof(*text));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(text, 0, sizeof(*text));
    sts = __pmDecodeText((__pmPDU *)text, &ident, &buffer);
    fprintf(stderr, "  __pmDecodeText: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(buffer); }

    fprintf(stderr, "[%s] checking large buflen field\n", name);
    memset(text, 0, sizeof(*text));
    text->hdr.len = sizeof(*text);
    text->hdr.type = PDU_TEXT;
    text->buflen = htonl(INT_MAX - 1);
    sts = __pmDecodeText((__pmPDU *)text, &ident, &buffer);
    fprintf(stderr, "  __pmDecodeText: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(buffer); }

    fprintf(stderr, "[%s] checking negative buflen field\n", name);
    memset(text, 0, sizeof(*text));
    text->hdr.len = sizeof(*text);
    text->hdr.type = PDU_TEXT;
    text->buflen = htonl(-2);
    sts = __pmDecodeText((__pmPDU *)text, &ident, &buffer);
    fprintf(stderr, "  __pmDecodeText: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(buffer); }

    fprintf(stderr, "[%s] checking access beyond buffer\n", name);
    memset(text, 0, sizeof(*text));
    text->hdr.len = sizeof(*text);
    text->hdr.type = PDU_TEXT;
    text->buflen = htonl(2);
    sts = __pmDecodeText((__pmPDU *)text, &ident, &buffer);
    fprintf(stderr, "  __pmDecodeText: sts = %d (%s)\n", sts, pmErrStr(sts));
    if (sts >= 0) { free(buffer); }

    free(text);
}

static void
decode_trace_ack(const char *name)
{
    int			sts, ack;
    struct trace_ack {
	__pmTracePDUHdr	hdr;
    } *trace_ack;

    trace_ack = (struct trace_ack *)malloc(sizeof(*trace_ack));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(trace_ack, 0, sizeof(*trace_ack));
    sts = __pmtracedecodeack((__pmPDU *)trace_ack, &ack);
    fprintf(stderr, "  __pmtracedecodeack: sts = %d (%s)\n", sts, pmtraceerrstr(sts));

    fprintf(stderr, "[%s] checking access beyond buffer\n", name);
    memset(trace_ack, 0, sizeof(*trace_ack));
    trace_ack->hdr.len = sizeof(*trace_ack);
    trace_ack->hdr.type = TRACE_PDU_ACK;
    sts = __pmtracedecodeack((__pmPDU *)trace_ack, &ack);
    fprintf(stderr, "  __pmtracedecodeack: sts = %d (%s)\n", sts, pmtraceerrstr(sts));

    free(trace_ack);
}

static void
decode_trace_data(const char *name)
{
    int			sts, len, type, p, *ip;
    char		*tag;
    double		data;
    struct trace_data {
	__pmTracePDUHdr	hdr;
	struct {
#ifdef HAVE_BITFIELDS_LTOR
	unsigned int    version  : 8;
	unsigned int    taglen   : 8;
	unsigned int    tagtype  : 8;
	unsigned int    protocol : 1;
	unsigned int    pad      : 7;
#else
	unsigned int    pad      : 7;
	unsigned int    protocol : 1;
	unsigned int    tagtype  : 8;
	unsigned int    taglen   : 8;
	unsigned int    version  : 8;
#endif
	} bits;
	double		value;
	char		tag[0];
    } *trace_data;

    trace_data = (struct trace_data *)malloc(sizeof(*trace_data));

    fprintf(stderr, "[%s] checking all-zeroes structure\n", name);
    memset(trace_data, 0, sizeof(*trace_data));
    sts = __pmtracedecodedata((__pmPDU *)trace_data, &tag, &len, &type, &p, &data);
    fprintf(stderr, "  __pmtracedecodedata: sts = %d (%s)\n", sts, pmtraceerrstr(sts));
    if (sts >= 0) { free(tag); }

    fprintf(stderr, "[%s] checking large taglen field\n", name);
    memset(trace_data, 0, sizeof(*trace_data));
    trace_data->hdr.len = sizeof(*trace_data);
    trace_data->hdr.type = TRACE_PDU_DATA;
    trace_data->bits.version = TRACE_PDU_VERSION;
    trace_data->bits.taglen = CHAR_MAX - 1;
    ip = (int *)&trace_data->bits;
    *ip = htonl(*ip);
    sts = __pmtracedecodedata((__pmPDU *)trace_data, &tag, &len, &type, &p, &data);
    fprintf(stderr, "  __pmtracedecodedata: sts = %d (%s)\n", sts, pmtraceerrstr(sts));
    if (sts >= 0) { free(tag); }

    fprintf(stderr, "[%s] checking negative taglen field\n", name);
    memset(trace_data, 0, sizeof(*trace_data));
    trace_data->hdr.len = sizeof(*trace_data);
    trace_data->hdr.type = TRACE_PDU_DATA;
    trace_data->bits.version = TRACE_PDU_VERSION;
    trace_data->bits.taglen = -1;
    ip = (int *)&trace_data->bits;
    *ip = htonl(*ip);
    sts = __pmtracedecodedata((__pmPDU *)trace_data, &tag, &len, &type, &p, &data);
    fprintf(stderr, "  __pmtracedecodedata: sts = %d (%s)\n", sts, pmtraceerrstr(sts));
    if (sts >= 0) { free(tag); }

    fprintf(stderr, "[%s] checking access beyond buffer\n", name);
    memset(trace_data, 0, sizeof(*trace_data));
    trace_data->hdr.len = sizeof(*trace_data);
    trace_data->hdr.type = TRACE_PDU_DATA;
    trace_data->bits.version = TRACE_PDU_VERSION;
    trace_data->bits.taglen = 2;
    ip = (int *)&trace_data->bits;
    *ip = htonl(*ip);
    sts = __pmtracedecodedata((__pmPDU *)trace_data, &tag, &len, &type, &p, &data);
    fprintf(stderr, "  __pmtracedecodedata: sts = %d (%s)\n", sts, pmtraceerrstr(sts));
    if (sts >= 0) { free(tag); }

    free(trace_data);
}

typedef void (*decode_t)(const char *);

struct pdu {
    const char *name;
    decode_t decode;
} pdus[] = {
    { "cred",		decode_creds },
    { "error",		decode_error },
    { "profile",	decode_profile },
    { "fetch",		decode_fetch },
    { "desc_req", 	decode_desc_req },
    { "desc", 		decode_desc },
    { "instance_req", 	decode_instance_req },
    { "instance", 	decode_instance },
    { "pmns_ids",	decode_pmns_ids },
    { "pmns_names",	decode_pmns_names },
    { "pmns_child",	decode_pmns_child },
    { "pmns_traverse",	decode_pmns_traverse },
    { "log_control",	decode_log_control },
    { "log_status",	decode_log_status },
    { "log_request",	decode_log_request },
    { "result", 	decode_result },
    { "text_req", 	decode_text_req },
    { "text", 		decode_text },
    { "trace_ack",	decode_trace_ack },
    { "trace_data",	decode_trace_data },
    { "attr",		decode_attr },
};

int
main(int argc, char **argv)
{
    int		c, d;
    int		sts, errflag = 0;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {
	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind < argc-1) {
	fprintf(stderr, "Usage: %s [-D n]\n", pmProgname);
	exit(1);
    }

    if (argc == optind) {
	for (d = 0; d < sizeof(pdus)/sizeof(struct pdu); d++)
	    pdus[d].decode(pdus[d].name);
    } else {
	for (c = 0; c < (argc - optind); c++)
	    for (d = 0; d < sizeof(pdus)/sizeof(struct pdu); d++)
		if (strcmp(argv[optind + c], pdus[d].name) == 0)
		    pdus[d].decode(pdus[d].name);
    }
    exit(0);
}
