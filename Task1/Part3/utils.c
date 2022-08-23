#include "wc.h"


extern struct team teams[NUM_TEAMS];
extern int test;
extern int finalTeam1;
extern int finalTeam2;

int processType = HOST;
const char *team_names[] = {
  "India", "Australia", "New Zealand", "Sri Lanka",   // Group A
  "Pakistan", "South Africa", "England", "Bangladesh" // Group B
};


void teamPlay(void)
{
	char buf[10];
	char inputfile[256];
	sprintf(inputfile, "test/%d/inp/%s", test, teams[processType].name);
	int fd = open(inputfile, O_RDONLY);
	if(fd < 0){
		perror("open");
		exit(-1);
	}
	while(1) {
		assert(read(fd, buf, 1) == 1);
		assert(write(teams[processType].matchpipe[1], buf, 1) == 1);
		assert(read(teams[processType].commpipe[0], buf, 1) == 1);
		if(buf[0] == '1') exit(0);
	}
	return;
}

void endTeam(int teamID)
{
	assert(write(teams[teamID].commpipe[1], "1", 1) == 1);
	return;
}

int match(int team1, int team2)
{
	char buf[10];
	assert(read(teams[team1].matchpipe[0], buf, 1) == 1);
	assert(write(teams[team1].commpipe[1], "0", 1) == 1);
	int x = buf[0] - '0';
	assert(read(teams[team2].matchpipe[0], buf, 1) == 1);
	assert(write(teams[team2].commpipe[1], "0", 1) == 1);
	int y = buf[0] - '0';
	if((x + y) % 2 == 0) {
		int temp = team1;
		team1 = team2;
		team2 = temp;
	}
	char outputfile[256];
	int len;
	if((team1 < 4) ^ (team2 < 4))
		len = sprintf(outputfile, "test/%d/out/%sv%s-Final", test, teams[team1].name, teams[team2].name);
	else
		len = sprintf(outputfile, "test/%d/out/%sv%s", test, teams[team1].name, teams[team2].name);
	int fd = open(outputfile, O_WRONLY|O_CREAT, 0644);
	if(fd < 0){
		perror("open");
		exit(-1);
	}
	char buf1[256];
	len = sprintf(buf1, "Innings1: %s bats\n", teams[team1].name);
	assert(write(fd, buf1, len) == len);
	int prevScore = 0, currScore = 0, wickets = 0;
	for(int i = 0; i < 120; i++) {
		assert(read(teams[team1].matchpipe[0], buf, 1) == 1);
		assert(write(teams[team1].commpipe[1], "0", 1) == 1);
		int x = buf[0] - '0';
		assert(read(teams[team2].matchpipe[0], buf, 1) == 1);
		assert(write(teams[team2].commpipe[1], "0", 1) == 1);
		int y = buf[0] - '0';
		if(x == y){
			wickets++;
			len = sprintf(buf1, "%d:%d\n", wickets, currScore - prevScore);
			assert(write(fd, buf1, len) == len);
			prevScore = currScore;
			if(wickets == 10) break;
		}
		else currScore += x;
	}
	if(wickets != 10) {
		len = sprintf(buf1, "%d:%d*\n", wickets + 1, currScore - prevScore);
		assert(write(fd, buf1, len) == len);
	}
	len = sprintf(buf1, "%s TOTAL: %d\n", teams[team1].name, currScore);
	assert(write(fd, buf1, len) == len);
	int team1Score = currScore;

	len = sprintf(buf1, "Innings2: %s bats\n", teams[team2].name);
	assert(write(fd, buf1, len) == len);
	prevScore = 0, currScore = 0, wickets = 0;
	for(int i = 0; i < 120; i++) {
		assert(read(teams[team2].matchpipe[0], buf, 1) == 1);
		assert(write(teams[team2].commpipe[1], "0", 1) == 1);
		int x = buf[0] - '0';
		assert(read(teams[team1].matchpipe[0], buf, 1) == 1);
		assert(write(teams[team1].commpipe[1], "0", 1) == 1);
		int y = buf[0] - '0';
		if(x == y){
			wickets++;
			len = sprintf(buf1, "%d:%d\n", wickets, currScore - prevScore);
			assert(write(fd, buf1, len) == len);
			prevScore = currScore;
			if(wickets == 10) break;
		}
		else currScore += x;
		if(currScore > team1Score)break;
	}
	if(wickets != 10) {
		len = sprintf(buf1, "%d:%d*\n", wickets + 1, currScore - prevScore);
		assert(write(fd, buf1, len) == len);
	}
	len = sprintf(buf1, "%s TOTAL: %d\n", teams[team2].name, currScore);
	assert(write(fd, buf1, len) == len);
	int winner;
	if(currScore > team1Score) {
		len = sprintf(buf1, "%s beats %s by %d wickets\n", teams[team2].name, teams[team1].name, 10 - wickets);
		winner = team2;
	}
	else if(currScore < team1Score) {
		len = sprintf(buf1, "%s beats %s by %d runs\n", teams[team1].name, teams[team2].name, team1Score - currScore);
		winner = team1;
	}
	else {
		if(team1 < team2) {
			len = sprintf(buf1, "TIE: %s beats %s\n", teams[team1].name, teams[team2].name);
			winner = team1;
		}
		else {
			len = sprintf(buf1, "TIE: %s beats %s\n", teams[team2].name, teams[team1].name);
			winner = team2;
		}
	}
	assert(write(fd, buf1, len) == len);
	return winner;
}

void spawnTeams(void)
{
	for(int i = 0; i < 8; i++) {
		strcpy(teams[i].name, team_names[i]);
   		if(pipe(teams[i].matchpipe) < 0){
        		perror("pipe");
        		exit(-1);
   		}
   		if(pipe(teams[i].commpipe) < 0){
        		perror("pipe");
        		exit(-1);
   		}
   		int pid = fork();
   		if(pid == 0) {
   			processType = i;
   			close(teams[i].matchpipe[0]);
   			close(teams[i].commpipe[1]);
   			teamPlay();
   		}
   		else {
   			close(teams[i].matchpipe[1]);
   			close(teams[i].commpipe[0]);
   		}
	}
	return;
}

void conductGroupMatches(void)
{
	int gpipe[2][2];
	for(int i = 0; i < 2; i++){
	   	if(pipe(gpipe[i]) < 0){
	        	perror("pipe");
	        	exit(-1);
	   	}
		int pid = fork();
		if(pid == 0) {
			int victories[8];
			for(int j = 0; j < 8; j++) victories[j] = 0;
			if(i == 0) {
				for(int x = 0; x < 4; x++) {
					for(int y = x + 1; y < 4; y++) {
						int winner = match(x, y);
						victories[winner]++;
					}
				}
				int winner, wins = -1;
				for(int j = 0; j < 4; j++) {
					if(wins < victories[j]) {
						winner = j;
						wins = victories[j];
					}
				}
				for(int j = 0; j < 4; j++) {
					if(j != winner) endTeam(j);
				}
				char buf[10];
				int len = sprintf(buf, "%d", winner);
				close(gpipe[0][0]);
				assert(write(gpipe[0][1], buf, len) == len);
				exit(0);
			}
			else {
				for(int x = 4; x < 8; x++) {
					for(int y = x + 1; y < 8; y++) {
						int winner = match(x, y);
						victories[winner]++;
					}
				}
				int winner, wins = -1;
				for(int j = 4; j < 8; j++) {
					if(wins < victories[j]) {
						winner = j;
						wins = victories[j];
					}
				}
				for(int j = 4; j < 8; j++) {
					if(j != winner) endTeam(j);
				}
				char buf[10];
				int len = sprintf(buf, "%d", winner);
				close(gpipe[1][0]);
				assert(write(gpipe[1][1], buf, len) == len);
				exit(0);
			}
		}
	}
	close(gpipe[0][1]);
	close(gpipe[1][1]);
	char buf[10];
	assert(read(gpipe[0][0], buf, 1) == 1);
	finalTeam1 = buf[0] - '0';
	assert(read(gpipe[1][0], buf, 1) == 1);
	finalTeam2 = buf[0] - '0';
	return;
}
