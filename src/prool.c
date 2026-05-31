// prool code for MUD
// proolix@gmail.com
// http://mud.virtustan.net
// Virtustan, Europe, 2026
// (c) GPL v.3

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

char *hostname(const char *ip) { // written by Copilot
    static char host[NI_MAXHOST];   // статичний буфер
    struct sockaddr_in sa;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;

    // Конвертац╕я IP - binary
    if (inet_pton(AF_INET, ip, &sa.sin_addr) != 1) {
        strcpy(host, "*");
        return host;
    }

    // Reverse DNS lookup
    int res = getnameinfo((struct sockaddr*)&sa, sizeof(sa),
                          host, sizeof(host),
                          NULL, 0, NI_NAMEREQD);

    if (res != 0) {
        strcpy(host, "*");
    }

    return host;
}

// vim: ts=4 sw=4 tw=0 noet syntax=cpp :
