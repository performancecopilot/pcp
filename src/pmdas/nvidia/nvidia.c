#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "domain.h"

#include <nvml.h>

#include <stdio.h>

// GCARD_INDOM struct, stats that are per card
typedef struct {

    int cardid;
    char *name;
    char *busid;
    int temp;
    int fanspeed;
    int perfstate;
    int gpuactive;
    int memactive;
    unsigned long long memused;
    unsigned long long memtotal;

} nvinfo_t;

// overall struct, holds instance values, indom arrays and instance struct arrays
typedef struct {

    int numcards;

    nvinfo_t *nvinfo;

    pmdaIndom *nvindom;

} pcp_nvinfo_t;

// each INDOM, this only has one, corresponding to values that change per card
pmdaIndom indomtab[] = {
#define GCARD_INDOM     0
    { GCARD_INDOM, 0, NULL },
};

static pcp_nvinfo_t pcp_nvinfo;


// list of metrics we want to export
// all in cluster 0, with increasing item ids
static pmdaMetric metrictab[] = {
/* num cards , no indom */
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* card id */
    { NULL, 
      { PMDA_PMID(0,1), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* card name */
    { NULL, 
      { PMDA_PMID(0,2), PM_TYPE_STRING, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* bus id */
    { NULL, 
      { PMDA_PMID(0,3), PM_TYPE_STRING, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* temp */
    { NULL, 
      { PMDA_PMID(0,4), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* fanspeed */
    { NULL, 
      { PMDA_PMID(0,5), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* perf state */
    { NULL, 
      { PMDA_PMID(0,6), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* gpu active */
    { NULL, 
      { PMDA_PMID(0,7), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mem active */
    { NULL, 
      { PMDA_PMID(0,8), PM_TYPE_U32, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },
/* mem used */
    { NULL, 
      { PMDA_PMID(0,9), PM_TYPE_U64, GCARD_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/* mem total */
    { NULL, 
      { PMDA_PMID(0,10), PM_TYPE_U64, GCARD_INDOM, PM_SEM_DISCRETE, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
};

static char	mypath[MAXPATHLEN];
static int	isDSO = 1;

int updatenv( pcp_nvinfo_t *pcp_nvinfo ) {

    nvmlReturn_t result;
    unsigned int device_count, i;

    result = nvmlInit();
    result = nvmlDeviceGetCount(&device_count);

    nvmlDevice_t device;
    char name[NVML_DEVICE_NAME_BUFFER_SIZE];
    nvmlPciInfo_t pci;
    unsigned int fanspeed;
    unsigned int temperature;
    nvmlUtilization_t utilization;
    nvmlMemory_t memory;
    nvmlPstates_t pstate;

    pcp_nvinfo->numcards = device_count;

    for (i = 0; i < device_count; i++){
        

        result = nvmlDeviceGetHandleByIndex(i, &device);

        result = nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
        
        result = nvmlDeviceGetPciInfo(device, &pci);

        result = nvmlDeviceGetFanSpeed(device, &fanspeed);

        result = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temperature);

        result = nvmlDeviceGetUtilizationRates(device, &utilization);

        result = nvmlDeviceGetMemoryInfo(device, &memory);

        result = nvmlDeviceGetPerformanceState(device, &pstate);

        pcp_nvinfo->nvinfo[i].cardid = i;
        if( pcp_nvinfo->nvinfo[i].name == NULL){
            pcp_nvinfo->nvinfo[i].name = strdup(name);
        }
        if(pcp_nvinfo->nvinfo[i].busid == NULL){
            pcp_nvinfo->nvinfo[i].busid = strdup(pci.busId);
        }
        pcp_nvinfo->nvinfo[i].temp = temperature;
        pcp_nvinfo->nvinfo[i].fanspeed = fanspeed;
        pcp_nvinfo->nvinfo[i].perfstate = pstate;
        pcp_nvinfo->nvinfo[i].gpuactive = utilization.gpu;
        pcp_nvinfo->nvinfo[i].memactive = utilization.memory;
        pcp_nvinfo->nvinfo[i].memused = memory.used;
        pcp_nvinfo->nvinfo[i].memtotal = memory.total;

    }

    result = nvmlShutdown();

    return 0;

}


static int
nvidia_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom) {

    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);

    updatenv( &pcp_nvinfo );

    if (idp->cluster != 0 || idp->item < 0 || idp->item > 10){
    	return PM_ERR_PMID;
    }
    else if ( (idp->item !=0) && (inst > indomtab[GCARD_INDOM].it_numinst)) {
	   return PM_ERR_INST;
    }

    //don't need to check cluster, since we only get here if 0 from above

    switch( idp->item ){
        case 0:
            atom->ul = pcp_nvinfo.numcards;
            break;
        case 1:
            atom->ul = pcp_nvinfo.nvinfo[inst].cardid;
            break;
        case 2:
            atom->cp = pcp_nvinfo.nvinfo[inst].name;
            break;
        case 3:
            atom->cp = pcp_nvinfo.nvinfo[inst].busid;
            break;
        case 4:
            atom->ul = pcp_nvinfo.nvinfo[inst].temp;
            break;
        case 5:
            atom->ul = pcp_nvinfo.nvinfo[inst].fanspeed;
            break;
        case 6:
            atom->ul = pcp_nvinfo.nvinfo[inst].perfstate;
            break;
        case 7:
            atom->ul = pcp_nvinfo.nvinfo[inst].gpuactive;
            break;
        case 8:
            atom->ul = pcp_nvinfo.nvinfo[inst].memactive;
            break;
        case 9:
            atom->ull = pcp_nvinfo.nvinfo[inst].memused;
            break;
        case 10:
            atom->ull = pcp_nvinfo.nvinfo[inst].memtotal;
            break;
        default:
            return PM_ERR_PMID;
    }

    
    return 0;
}

/**
 * Initializes the path to the help file for this PMDA.
 */
static void initializeHelpPath() {
    int sep = __pmPathSeparator();
    snprintf(mypath, sizeof(mypath), "%s%c" "nvidia" "%c" "help",
            pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
}

void 
nvidia_init(pmdaInterface *dp)
{

    if (isDSO) {
    	initializeHelpPath();
    	pmdaDSO(dp, PMDA_INTERFACE_2, "nvidia DSO", mypath);
    }

    if (dp->status != 0)
	return;

    nvmlReturn_t result;
    result = nvmlInit();
    if (NVML_SUCCESS != result){
        dp->status = -EIO;
        return;
    }

    // Initialize instance domain and instances.
    unsigned int device_count;
    result = nvmlDeviceGetCount(&device_count);

    pmdaIndom *idp = &indomtab[GCARD_INDOM];
    pcp_nvinfo.nvindom = idp;
    pcp_nvinfo.nvindom->it_numinst = device_count;
    pcp_nvinfo.nvindom->it_set = (pmdaInstid *)malloc(device_count * sizeof(pmdaInstid));

    char gpuname[32];
    int i;

    for(i=0 ; i<device_count ; i++){
        pcp_nvinfo.nvindom->it_set[i].i_inst = i;
        snprintf( gpuname, sizeof(gpuname), "gpu%d", i);
        pcp_nvinfo.nvindom->it_set[i].i_name = strdup(gpuname);
    }

    int nvsize = pcp_nvinfo.nvindom->it_numinst * sizeof(nvinfo_t);
    pcp_nvinfo.nvinfo = (nvinfo_t*)malloc(nvsize);
    memset(pcp_nvinfo.nvinfo, 0, nvsize);

    pcp_nvinfo.numcards = 0;

    // Set fetch callback function.
    pmdaSetFetchCallBack(dp, nvidia_fetchCallBack);

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), 
	     metrictab, sizeof(metrictab)/sizeof(metrictab[0]));

}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n",
	      stderr);		
    exit(1);
}

int
main(int argc, char **argv)
{
    int			err = 0;
    pmdaInterface	desc;

    isDSO = 0;
    __pmSetProgname(argv[0]);

    initializeHelpPath();
    pmdaDaemon(&desc, PMDA_INTERFACE_2, pmProgname, NVML,
		"nvidia.log", mypath);

    if (pmdaGetOpt(argc, argv, "D:d:l:?", &desc, &err) != EOF)
    	err++;
    if (err)
    	usage();

    pmdaOpenLog(&desc);
    nvidia_init(&desc);
    pmdaConnect(&desc);
    pmdaMain(&desc);

    exit(0);
}
