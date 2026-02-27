// prool code for MUD
// proolix@gmail.com
// http://mud.virtustan.net
// Virtustan, Europe, 2026
// (c) GPL v.3

#if 0	
void mssp_start(DESCRIPTOR_DATA * t) // by prool
{char buf[1024];

#if 0
const char mssp_str[] = {IAC,SB,MSSP,
	MSSP_VAR,'P','L','A','Y','E','R','S',MSSP_VAL,'0',
	MSSP_VAR,'N','A','M','E',MSSP_VAL,'V','i','r','t','u','s','t','a','n',' ','M','U','D',
	IAC,SE,'\0'};
#endif

sprintf(buf,
"%c%c%c%cPLAYERS%c%i%cNAME%cVirtustan MUD%cUPTIME%c%li%cCRAWL DELAY%c-1\
%cHOSTNAME%cmud.kharkov.org\
%cPORT%c3000\
%cPORT%c3001\
%cCODEBASE%cCircleMUD/Byliny\
%cCONTACT%cproolix@gmail.com\
%cCREATED%c2007\
%cIP%c95.217.157.136\
%cLANGUAGE%crussian\
%cLOCATION%cEurope\
%cMINIMUM AGE%c0\
%cWEBSITE%chttp://mud.kharkov.org\
%cFAMILY%cDikuMUD\
%cAREAS%c%i\
%cMOBILES%c%i\
%cOBJECTS%c%i\
%cROOMS%c%i\
%cCLASSES%c15\
%cRACES%c6\
%cANSI%c1\
%cMCCP%c1\
%cMCP%c0\
%cMSP%c0\
%cMXP%c0\
%cPUEBLO%c0\
%cGMCP%c0\
%cMSDP%c0\
%cHIRING BUILDERS%c1\
%cHIRING CODERS%c1\
%cPLAYER CLANS%c1\
%cWORLD ORIGINALITY%c1\
%cGENRE%cFantasy\
%cGAMESYSTEM%cCustom\
%cLEVELS%c30\
%cVT100%c0\
%cPAY TO PLAY%c0\
%cPAY FOR PERKS%c0\
%cINTERMUD%c0\
%cXTERM 256 COLORS%c0\
%cXTERM TRUE COLORS%c0\
%cUTF-8%c1\
%cCHARSET%cUTF-8%ckoi8-r%ccp1251\
%cREFERRAL%ctbamud.com:4000%ctbamud.com:9091\
%c%c",
IAC,SB,MSSP,MSSP_VAR,MSSP_VAL,total_players,MSSP_VAR,MSSP_VAL,MSSP_VAR,MSSP_VAL,(long int)boot_time,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,top_of_zone_table + 1 /*statistic_zones*/,
MSSP_VAR,MSSP_VAL,top_of_mobt + 1 /*statistic_mobs*/,
MSSP_VAR,MSSP_VAL,top_of_objt + 1 /*statistic_objs*/,
MSSP_VAR,MSSP_VAL,top_of_world + 1 /*statistic_rooms*/,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR,MSSP_VAL,
MSSP_VAR, MSSP_VAL, MSSP_VAL, MSSP_VAL,
MSSP_VAR, MSSP_VAL, MSSP_VAL,
IAC,SE);

// printf("MSSP total_players %i\n",total_players);

write_to_descriptor(t->descriptor, buf, strlen(buf));
}
#endif

char *koi_to_lat(char *str_i, char *str_o) // by prool, v.2
{
char *p;

p=str_o;

if ((str_i==0)||(str_o==0)) return NULL;
while (*str_i)
{
	switch (*str_i)
	{
		case 'А': *str_o='A'; break;
		case 'Б': *str_o='B'; break;
		case 'В': *str_o='V'; break;
		case 'Г': *str_o='G'; break;
		case 'Д': *str_o='D'; break;
		case 'Е': *str_o='E'; break;
		case 'Ё': *str_o='E'; break;
		case 'Ж': *str_o='*'; break;
		case 'З': *str_o='Z'; break;
		case 'И': *str_o='I'; break;
		case 'Й': *str_o='J'; break;
		case 'К': *str_o='K'; break;
		case 'Л': *str_o='L'; break;
		case 'М': *str_o='M'; break;
		case 'Н': *str_o='N'; break;
		case 'О': *str_o='O'; break;
		case 'П': *str_o='P'; break;
		case 'Р': *str_o='R'; break;
		case 'С': *str_o='S'; break;
		case 'Т': *str_o='T'; break;
		case 'У': *str_o='U'; break;
		case 'Ф': *str_o='F'; break;
		case 'Х': *str_o='H'; break;
		case 'Ц': *str_o='C'; break;
		case 'Ч': *str_o='4'; break;
		case 'Ш': *str_o='W'; break;
		case 'Ь': *str_o='\''; break;
		case 'Ъ': *str_o='"'; break;
		case 'Ы': *str_o='#'; break;
		case 'Э': *str_o='E'; break;
		case 'Ю': *str_o='Y'; break;
		case 'Я': *str_o='9'; break;
		case 'а': *str_o='a'; break;
		case 'б': *str_o='b'; break;
		case 'в': *str_o='v'; break;
		case 'г': *str_o='g'; break;
		case 'д': *str_o='d'; break;
		case 'е': *str_o='e'; break;
		case 'ё': *str_o='e'; break;
		case 'ж': *str_o='*'; break;
		case 'з': *str_o='z'; break;
		case 'и': *str_o='i'; break;
		case 'й': *str_o='j'; break;
		case 'к': *str_o='k'; break;
		case 'л': *str_o='l'; break;
		case 'м': *str_o='m'; break;
		case 'н': *str_o='n'; break;
		case 'о': *str_o='o'; break;
		case 'п': *str_o='p'; break;
		case 'р': *str_o='r'; break;
		case 'с': *str_o='s'; break;
		case 'т': *str_o='t'; break;
		case 'у': *str_o='u'; break;
		case 'ф': *str_o='f'; break;
		case 'х': *str_o='h'; break;
		case 'ц': *str_o='c'; break;
		case 'ч': *str_o='4'; break;
		case 'ш': *str_o='w'; break;
		case 'щ': *str_o='w'; break;
		case 'ь': *str_o='\''; break;
		case 'ъ': *str_o='"'; break;
		case 'ы': *str_o='#'; break;
		case 'э': *str_o='e'; break;
		case 'ю': *str_o='y'; break;
		case 'я': *str_o='9'; break;
		default: *str_o=*str_i;
	}
	str_i++;
	str_o++;
}
*str_o=0;
return p;
}

char *ptime(void) // by prool. Возвращаемое значение: ссылка на текстовую строку с текущим временем
	{
	char *tmstr;
	time_t mytime;

	mytime = time(0);

	tmstr = (char *) asctime(localtime(&mytime));
	*(tmstr + strlen(tmstr) - 1) = '\0';

	return tmstr;

	}

void prool_make_www (int players)
{
FILE *fp;

//printf("%s players %i\r\n", ptime(), players);

fp=fopen("proolstat.txt", "w");
if (fp==0) return;
fprintf(fp, "time %s\r\nplayers %i\r\n", ptime(), players);
fclose(fp);
}
