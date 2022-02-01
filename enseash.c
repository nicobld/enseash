#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/resource.h>

#define MAXBUF 256

#define DEBUG write(STDERR_FILENO,"debug",6);

char bufmsg[MAXBUF];
int bgtot = 0; //nombre de process en background

struct bgps {
	pid_t pid;
	int stat;
	char name[128];
	struct timespec start;
	struct timespec current;
};

struct bgps bgps[MAXBUF]; //liste des processus en background

void command(char prompt[MAXBUF]);
void split(char prompt[MAXBUF]);

int main(int argc, char* argv[]){
	write(STDOUT_FILENO,"Bienvenue dans le Shell ENSEA.\nPour quitter, tapez 'exit'.\n",60);

	char prompt[MAXBUF];
	int promptlen;

	while(1){
		if(bgtot>0) { //si on a des process en background on affiche combien y'en a 
			snprintf(bufmsg,8,"[%d&]",bgtot);
			write(STDOUT_FILENO,bufmsg,strlen(bufmsg));
		}
		write(STDOUT_FILENO,"enseash % ",11);
		if((promptlen = read(STDIN_FILENO,prompt,MAXBUF))==-1) {
			perror("read");
			exit(EXIT_FAILURE);
		}
		prompt[promptlen-1]='\0';// on remplace le \n par \0
		command(prompt);
	}

	exit(EXIT_SUCCESS);
	return 0;
}

void command(char prompt[MAXBUF]){

	if(!strncmp(prompt,"exit",5)){ //pour exit
		write(STDOUT_FILENO,"Ciao\n",6);
		exit(EXIT_SUCCESS);
	}

	int background = (prompt[strlen(prompt)-1] == '&'); // process en background ou pas


	pid_t pid = fork();

	if(!strncmp(prompt,"enseaps",8)&&pid==0){ //le code pour afficher les ps en background
		write(STDOUT_FILENO,"Les process en background :\n",29);
		write(STDOUT_FILENO,"PID\tNAME\t\tTIME\n",15);
		for(int l=0;l<bgtot;l++){
			snprintf(bufmsg,2*MAXBUF,"%d\t%s\t%ld\n",bgps[l].pid,bgps[l].name,bgps[l].current.tv_sec-bgps[l].start.tv_sec);
			write(STDOUT_FILENO,bufmsg,strlen(bufmsg));
		}
		exit(EXIT_SUCCESS);
	}


	int stat;

	struct timespec start, end;
	if (pid==0){
		split(prompt);
	}
	if(background){ //si le process est en background on affiche son PID et on commence le timer
		snprintf(bufmsg,16,"pid: %d\n",pid);
		write(STDOUT_FILENO,bufmsg,strlen(bufmsg));

		bgps[bgtot].pid = pid; //le pid
		strncpy(bgps[bgtot].name,prompt,MAXBUF); //le nom
		clock_gettime(CLOCK_MONOTONIC,&bgps[bgtot].start); // le debut du timer
		bgtot++;
	}else{
		clock_gettime(CLOCK_MONOTONIC,&start); //sinon le timer c'est juste une variable si on est pas en background
	}

	int waitpidvalue;

	for(int k=0;k<bgtot;k++){ //pour chaque process en background on revérifie leur état pour savoir s'ils sont mort
		//waitpid retourne > 0 si un enfant change d'état
		waitpidvalue = waitpid(bgps[k].pid,&bgps[k].stat,WNOHANG);//WNOHANG pour pas attendre juste on actualise l'état stat
		clock_gettime(CLOCK_MONOTONIC,&bgps[k].current);//on actualise leur timer
		if (WIFEXITED(bgps[k].stat)&&waitpidvalue>0){ // mort naturelle
			snprintf(bufmsg,MAXBUF,"[%d]+ Exit:%d Time:%ld\t%s\n\n",k,WEXITSTATUS(bgps[k].stat),bgps[k].current.tv_sec-bgps[k].start.tv_sec,bgps[k].name);
			write(STDOUT_FILENO,bufmsg,strlen(bufmsg));
			bgtot--;
		}
		else if (WIFSIGNALED(bgps[k].stat)&&waitpidvalue>0){ //mort par signal
			snprintf(bufmsg,2*MAXBUF,"[%d]+ Sign:%d Time:%ld\t%s\n\n",k,WTERMSIG(bgps[k].stat),bgps[k].current.tv_sec-bgps[k].start.tv_sec,bgps[k].name);
			write(STDOUT_FILENO,bufmsg,strlen(bufmsg));
			bgtot--;
		}
	}



	if(!background){ //si on est pas en background
		waitpid(pid,&stat,0); // on attend la mort du fils

		clock_gettime(CLOCK_MONOTONIC,&end);//fin du temps

		long total_time = (end.tv_sec-start.tv_sec)*1000+(end.tv_nsec-start.tv_nsec)/1000000;
		char stime[32];
		snprintf(stime,32,"%ld",total_time);

		if (WIFEXITED(stat)){ //mort naturelle
			sprintf(bufmsg,"[exit:%d|%sms]",WEXITSTATUS(stat),stime);
		}
		else if (WIFSIGNALED(stat)){ //mort par signal
			sprintf(bufmsg,"[sign:%d|%sms]",WTERMSIG(stat),stime);
		}

		write(STDOUT_FILENO,bufmsg,strlen(bufmsg));
	}
}

void split(char prompt[MAXBUF]){//fonction importante qui execute les commandes
	char delim[] = " ";
	char *ptr = strtok(prompt,delim);

	char* vector[MAXBUF];
	int i = 0;

	while(ptr != NULL){
		// le split en question
		vector[i]=malloc(MAXBUF*sizeof(char));
		strncpy(vector[i],ptr,MAXBUF);
		ptr = strtok(NULL,delim);
		i++;
	}

	if(!strncmp(vector[i-1],"&",2)){
		i--;
	}

	vector[i]=(char*)NULL;

	// On a créé un vecteur qui contient chaque commande séparée par un espace

	int last_pipe=0;
	
	for(int j=0;j<i;j++){
		if(!strncmp(vector[j],">",2)){// on trouve ">", le string après sera le fichier créé.
			int file1 = open(vector[j+1],O_CREAT|O_WRONLY,0666);
			dup2(file1,STDOUT_FILENO);
			vector[j]=(char*)NULL;
		}
		else if(!strncmp(vector[j],"<",2)){
			int file2 = open(vector[j+1],O_RDONLY);
			dup2(file2,STDIN_FILENO);
			vector[j] = (char*)NULL;
		}
		else if(!strncmp(vector[j],"|",2)){ //le pipe |
			int pipefd[2];
			pipe(pipefd);
			pid_t pid = fork();
			if (pid==0){ //l'enfant execute la 1ère commande
				close(pipefd[0]);//pas besoin de lire la fifo
				dup2(pipefd[1],STDOUT_FILENO);
				vector[j] = (char*)NULL;
				execvp(vector[last_pipe],vector+last_pipe);//on execute la commande jusqu'à |
				//si la commande existe pas on exit
				close(pipefd[1]);
				write(STDOUT_FILENO,"Wrong command\n",15);
				exit(EXIT_FAILURE);
			} else { //le père lit le pipe du fils et met le resultat en STDIN
				close(pipefd[1]);
				waitpid(pid,NULL,0);
				dup2(pipefd[0],STDIN_FILENO);
				close(pipefd[0]);
			}
			last_pipe = j+1;
		}
		else if (!strncmp(vector[j],"&&",3)){
			pid_t pid = fork();
			int stat;
			if (pid==0){
				vector[j]=(char*)NULL;
				execvp(vector[last_pipe],vector+last_pipe);
				write(STDOUT_FILENO,"Wrong command\n",15);
				exit(EXIT_FAILURE);
			} else {
				waitpid(pid,&stat,0);
				if(WIFEXITED(stat)){
					if(WEXITSTATUS(stat)!=0){
						exit(EXIT_FAILURE);
					}
				}else exit(EXIT_FAILURE);
			} last_pipe = j+1;
		}
	}

	execvp(vector[last_pipe],vector+last_pipe);
	write(STDERR_FILENO,"Wrong command\n",15);
	exit(EXIT_FAILURE);
}
