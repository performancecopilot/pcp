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

static void new_pmctype(configuration_t *conf)
{
    if(NULL == conf)
    {
        return;
    }

    ++conf->nConfigEntries;
    conf->configArr = realloc(conf->configArr, conf->nConfigEntries * sizeof *conf->configArr);

    if(NULL == conf->configArr) 
    {
        conf->nConfigEntries = 0;
        return;
    }
    memset(&conf->configArr[conf->nConfigEntries-1], 0, sizeof *conf->configArr);
}

static void add_pmctype(configuration_t *config, char *name)
{
    pmcconfiguration_t *entry;
    pmctype_t *newpmctype;

    if(!(config && name))
    {
        return;
    }
    if(0 == config->nConfigEntries) 
    {
        return;
    }
    entry = &config->configArr[config->nConfigEntries-1];

    newpmctype = malloc(sizeof *newpmctype);
    newpmctype->name = strdup(name);
    newpmctype->next = entry->pmcTypeList;
    entry->pmcTypeList = newpmctype;
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

    pmcsetting = config->configArr[config->nConfigEntries-1].pmcSettingList;

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
    pmcsetting_t *pmcSettingDel;

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
    free(config->configArr);
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

^\[                                   { BEGIN(PMCTYPELIST); new_pmctype(yyextra); /* a '[' char at beginning of line signals start of a list of PMC types */ }

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
