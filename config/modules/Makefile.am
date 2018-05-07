## Process this file with automake to produce Makefile.in

dist_moduleconf_DATA = cicero.conf espeak.conf espeak-ng.conf festival.conf \
		       flite.conf ibmtts.conf ivona.conf dtk-generic.conf \
                       epos-generic.conf espeak-generic.conf \
                       espeak-ng-mbrola-generic.conf espeak-mbrola-generic.conf \
                       llia_phon-generic.conf \
                       pico-generic.conf swift-generic.conf

dist_moduleconforig_DATA = cicero.conf espeak.conf festival.conf espeak-ng.conf
	                   flite.conf ibmtts.conf ivona.conf dtk-generic.conf \
                           epos-generic.conf espeak-generic.conf \
                           espeak-ng-mbrola-generic.conf espeak-mbrola-generic.conf \
                           llia_phon-generic.conf \
                           pico-generic.conf swift-generic.conf

if pico_support
dist_moduleconf_DATA += pico.conf
dist_moduleconforig_DATA += pico.conf
endif

if baratinoo_support
dist_moduleconf_DATA += baratinoo.conf
dist_moduleconforig_DATA += baratinoo.conf
endif

if kali_support
dist_moduleconf_DATA += kali.conf
dist_moduleconforig_DATA += kali.conf
endif

-include $(top_srcdir)/git.mk
