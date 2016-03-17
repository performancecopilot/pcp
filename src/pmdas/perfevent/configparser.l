/*
 * flex script used to generate a configuration file parser
 * Copyright (C) 2013 Joe White
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

%{
#include "configparser.h"

static int is_derived(char *name)
{
    char *str = NULL;

    str = strchr(name, ':');
    if (!str)
         return 0;
    if (!strcmp(str, ":derived"))
         return 1;
    return 0;
}

static void add_derived(configuration_t *config, char *name)
{
    pmcderived_t *entry;
    char *ptr;

    if (!name)
         return;
    ++config->nDerivedEntries;

    config->derivedArr = realloc(config->derivedArr, config->nDerivedEntries * sizeof *config->derivedArr);

    if(NULL == config->derivedArr)
    {
        config->nDerivedEntries = 0;
        return;
    }

    ptr = strchr(name, ':');
    *ptr = '\0';
    memset(&config->derivedArr[config->nDerivedEntries-1], 0, sizeof *config->derivedArr);

    entry = &config->derivedArr[config->nDerivedEntries-1];

    entry->name = strdup(name);
    entry->nsettings = 0;
    entry->derivedSettingList = NULL;
    context_derived = 1;
}

static void add_pmctype(configuration_t *config, char *name)
{
    pmcconfiguration_t *entry;
    pmctype_t *newpmctype;

    if(!(config && name))
    {
        return;
    }
    if (is_derived(name))
        return add_derived(config, name);

    if (context_newpmc)
    {
        ++config->nConfigEntries;
        config->configArr = realloc(config->configArr, config->nConfigEntries * sizeof *config->configArr);
        memset(&config->configArr[config->nConfigEntries-1], 0, sizeof *config->configArr);
    }
    if(NULL == config->configArr)
    {
        config->nConfigEntries = 0;
        return;
    }

    entry = &config->configArr[config->nConfigEntries-1];
    newpmctype = malloc(sizeof *newpmctype);
    newpmctype->name = strdup(name);
    newpmctype->next = entry->pmcTypeList;
    entry->pmcTypeList = newpmctype;
    context_derived = 0;
    context_newpmc = 0;
}

static void add_pmc_setting_name_derived(configuration_t *config, char *name)
{
    pmcderived_t *entry;
    pmcsetting_t *slist, *newpmcderivedsetting;

    if (0 == config->nDerivedEntries)
    {
        return;
    }
    entry = &config->derivedArr[config->nDerivedEntries - 1];
    newpmcderivedsetting = calloc(1, sizeof *newpmcderivedsetting);
    newpmcderivedsetting->name = strdup(name);
    newpmcderivedsetting->cpuConfig = CPUCONFIG_EACH_CPU;
    newpmcderivedsetting->next = NULL;

    slist = entry->derivedSettingList;
    if (slist == NULL)
    {
        entry->derivedSettingList = newpmcderivedsetting;
    }
    else
    {
        while(slist->next)
        {
            slist = slist->next;
        }
        slist->next = newpmcderivedsetting;
    }
    entry->nsettings++;
}

static void add_pmcsetting_name(configuration_t *config, char *name)
{
    pmcconfiguration_t *entry;
    pmcsetting_t *newpmcsetting;

    if(!(config && name))
    {
        return;
    }
    if(0 == config->nConfigEntries) 
    {
        return;
    }

    if (context_derived)
         return add_pmc_setting_name_derived(config, name);

    entry = &config->configArr[config->nConfigEntries-1];

    newpmcsetting = malloc(sizeof *newpmcsetting);
    newpmcsetting->name = strdup(name);
    newpmcsetting->cpuConfig = CPUCONFIG_EACH_CPU;
    newpmcsetting->next = entry->pmcSettingList;
    entry->pmcSettingList = newpmcsetting;
}

static void set_pmcsetting_cpuconfig(configuration_t *config, int cpuconfig)
{
    pmcsetting_t *pmcsetting;

    if( (NULL == config) || (0 == config->nConfigEntries) )
    {
        return;
    }

    if (context_derived)
    {
        pmcsetting = config->derivedArr[config->nDerivedEntries-1].derivedSettingList;
        while(pmcsetting->next)
        {
            pmcsetting = pmcsetting->next;
        }
    }
    else
    {
        pmcsetting = config->configArr[config->nConfigEntries-1].pmcSettingList;
    }


    if( NULL == pmcsetting )
    {
        return;
    }

    pmcsetting->cpuConfig = cpuconfig;
}

#ifdef DEBUG_PRINT_CONFIG
static void printconfig(configuration_t *config)
{
    int i;
    pmctype_t *pmcType;
    pmcsetting_t *pmcSetting;

    if((NULL == config) || (0 == config->nConfigEntries) )
    {
        fprintf(stderr,"Error null or empty configuration\n");
        return;
    }

    for(i = 0; i < config->nConfigEntries; ++i)
    {
        pmcType = config->configArr[i].pmcTypeList;
        fprintf(stderr,"PMCTYPES: ");
        while(pmcType) {
            if(pmcType->name) {
                fprintf(stderr,"%s ", pmcType->name);
            } else {
                fprintf(stderr,"ERROR name is null ");
            }
            pmcType = pmcType->next;
        }
        pmcSetting = config->configArr[i].pmcSettingList;
        fprintf(stderr,"\nSETTINGS: ");
        while(pmcSetting) {
            if(pmcSetting->name) {
                fprintf(stderr,"%s ", pmcSetting->name);
            } else {
                fprintf(stderr,"ERROR name is null ");
            }
            pmcSetting = pmcSetting->next;
        }
        fprintf(stderr,"\n");
    }
}
#endif

void free_configuration(configuration_t *config)
{
    int i;
    pmctype_t *pmcTypeDel;
    pmcsetting_t *pmcSettingDel, *tmp;

    if(NULL == config)
    {
        return;
    }

    for(i = 0; i < config->nConfigEntries; ++i)
    {
        while(config->configArr[i].pmcTypeList) 
        {
            pmcTypeDel = config->configArr[i].pmcTypeList;
            config->configArr[i].pmcTypeList = pmcTypeDel->next;

            free(pmcTypeDel->name);
            free(pmcTypeDel);
        }

        while(config->configArr[i].pmcSettingList) 
        {
            pmcSettingDel = config->configArr[i].pmcSettingList;
            config->configArr[i].pmcSettingList = pmcSettingDel->next;
            free(pmcSettingDel->name);
            free(pmcSettingDel);
        }
    }

    for(i = 0; i < config->nDerivedEntries; ++i)
    {
        tmp = pmcSettingDel = config->derivedArr[i].derivedSettingList;
        while(tmp != NULL)
        {
            tmp = tmp->next;
            free(pmcSettingDel);
            pmcSettingDel = tmp;
        }

        if (config->derivedArr[i].name)
            free(config->derivedArr[i].name);
    }
    free(config->configArr);
    free(config->derivedArr);
    free(config);
}

%}

%option reentrant
%option noyywrap
%option nounput
%option noinput
%option yylineno
%option extra-type="configuration_t *"

%s PMCSETTINGLIST
%s PMCTYPELIST
%%
#[^\n]*                               ; /* ignore comments */ 
[ \t\r]                               ; /* ignore whitespace */
\n                                    BEGIN(INITIAL); /* new-line always resets state machine */

^\[                                   { BEGIN(PMCTYPELIST); context_newpmc = 1;/* a '[' char at beginning of line signals start of a list of PMC types */ }

<PMCTYPELIST>{
\"[^\"]*\"                            { /* strip quotes */ yytext[strlen(yytext)-1] = '\0'; add_pmctype(yyextra, &yytext[1] ); /* allow any char in quotes except the quote char */  }
[^\"\][:blank:]]+                     add_pmctype(yyextra, yytext ); /* otherwise whitespace seperated tokens */
\]                                    BEGIN(INITIAL); /* a ']' is end of PMC type list */
}

^([[:alpha:]]+[[:alnum:][:punct:]]*) { BEGIN(PMCSETTINGLIST); add_pmcsetting_name(yyextra, yytext ); }

<PMCSETTINGLIST>{
cpu           set_pmcsetting_cpuconfig(yyextra, CPUCONFIG_EACH_CPU);
cpu_rr        set_pmcsetting_cpuconfig(yyextra, CPUCONFIG_ROUNDROBIN_CPU);
node          set_pmcsetting_cpuconfig(yyextra, CPUCONFIG_EACH_NUMANODE);
node_rr       set_pmcsetting_cpuconfig(yyextra, CPUCONFIG_ROUNDROBIN_NUMANODE);
[0-9]*        set_pmcsetting_cpuconfig(yyextra, atoi(yytext) );
}

<*>.|\n { fprintf(stderr, "Syntax error on line: %d \n", yylineno); return -1; }

%%

configuration_t *parse_configfile(const char *filename)
{
    FILE *fp;
    yyscan_t scanner;
    configuration_t *config;
    int ret;
    
    if(NULL == filename) {
        fprintf(stderr, "Error must specify a configuration file\n");
        return 0;
    }

    fp = fopen(filename, "r");
    if(NULL == fp) {
        fprintf(stderr, "Error opening config file\n");
        return 0;
    }

    config = malloc(sizeof *config);
    config->configArr = NULL;
    config->nConfigEntries = 0;
    config->derivedArr = NULL;
    config->nDerivedEntries = 0;

    yylex_init(&scanner);
    yyset_extra(config, scanner);
    yyset_in(fp, scanner);
    ret = yylex(scanner);
    yylex_destroy(scanner);

    fclose(fp);

    if(ret) {
        free_configuration(config);
        return 0;
    }
    
    return config;
}
