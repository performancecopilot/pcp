# Created by: Markiyan Kushnir <markiyan.kushnir@gmail.com>
# $FreeBSD:$
PORTNAME=	pcp
PORTVERSION=	3.11.0.1
PORTREVISION=	1
CATEGORIES=	sysutils

MAINTAINER=	markiyan.kushnir@gmail.com
COMMENT=	Performance Co-Pilot (PCP): framework and services to support system-level performance monitoring and management.

LICENSE_COMB=	dual
LICENSE=	GPLv2 LGPL21

USE_GITHUB=	yes
GH_ACCOUNT=	performancecopilot
GH_TAGNAME=	cf5e23a93d88420bca29b5be206b800845e55546

GNU_CONFIGURE=	yes
CONFIGURE_ARGS=	--localstatedir=${PREFIX} \
		    --with-logdir=/var/log/pcp \
		    --with-rundir=/var/run/pcp \
		    --with-tmpdir=/var/tmp \
		    --with-configdir=${PREFIX}/etc/pcp \
		    --with-rcdir=${PREFIX}/etc/rc.d

USES=		libtool gmake python bison

MAKEFILE=	GNUmakefile
ALL_TARGET=	default
MAKE_ENV=	DIST_ROOT=${STAGEDIR}

NO_MTREE=	yes

.include <bsd.port.mk>
