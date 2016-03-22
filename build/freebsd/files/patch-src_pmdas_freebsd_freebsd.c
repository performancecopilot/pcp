diff --git a/src/pmdas/freebsd/freebsd.c b/src/pmdas/freebsd/freebsd.c
index c3284bf..b1ca658 100644
--- src/pmdas/freebsd/freebsd.c
+++ src/pmdas/freebsd/freebsd.c
@@ -109,7 +109,7 @@ static pmdaMetric metrictab[] = {
 	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
     { (void *)"kernel.all.hz",
       { PMDA_PMID(CL_SYSCTL,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
-	PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0) } },
+	PMDA_PMUNITS(0,-1,0,0,PM_TIME_SEC,0) } },
     { (void *)"hinv.cpu.vendor",
       { PMDA_PMID(CL_SYSCTL,15), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
 	PMDA_PMUNITS(0,0,0,0,0,0) } },
@@ -176,7 +176,7 @@ static pmdaMetric metrictab[] = {
       { PMDA_PMID(CL_SPECIAL,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
 	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },
     { (void *)"mem.util.bufmem",
-      { PMDA_PMID(CL_SPECIAL,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
+      { PMDA_PMID(CL_SPECIAL,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
 	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },
     { (void *)"mem.util.cached",
       { PMDA_PMID(CL_SPECIAL,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
@@ -596,9 +596,9 @@ freebsd_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
 		break;
 
 	    case 7:	/* mem.util.bufmem */
-		sts = do_sysctl(mp, sizeof(atom->ul));
+		sts = do_sysctl(mp, sizeof(atom->ull));
 		if (sts > 0) {
-		    atom->ul = *((__uint32_t *)mp->m_data) / 1024;
+		    atom->ull = *((__uint64_t *)mp->m_data) / 1024;
 		    sts = 1;
 		}
 		break;
