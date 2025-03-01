#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "GrilleSDL.h"
#include "Ressources.h"

// Dimensions de la grille de jeu
#define NB_LIGNE 21
#define NB_COLONNE 17

// Macros utilisees dans le tableau tab
#define VIDE         0
#define MUR          1
#define PACMAN       2
#define PACGOM       3
#define SUPERPACGOM  4
#define BONUS        5
#define FANTOME      6

// Autres macros
#define LENTREE 15
#define CENTREE 8

typedef struct
{
  int L;
  int C;
  int couleur;
  int cache;
} S_FANTOME;

typedef struct {
  int presence;
  pthread_t tid;
} S_CASE;


int dir = GAUCHE;
S_CASE tab[NB_LIGNE][NB_COLONNE];
pthread_mutex_t mutexTab;
pthread_t tidPacMan, tidEvent;

void DessineGrilleBase();
void Attente(int milli);
void setTab(int l, int c, int presence = VIDE, pthread_t tid = 0);
void* ThreadPacMan(void* p);
void* ThreadEvent(void* p);

void handler_SIGINT(int sig);
void handler_SIGHUP(int sig);
void handler_SIGUSR1(int sig);
void handler_SIGUSR2(int sig);

///////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc,char* argv[])
{
  sigset_t mask;
  struct sigaction sigAct;
  char ok;
  pthread_mutex_init(&mutexTab,NULL);
  


  srand((unsigned)time(NULL));

  // Ouverture de la fenetre graphique
  printf("(MAIN%p) Ouverture de la fenetre graphique\n",pthread_self()); fflush(stdout);
  if (OuvertureFenetreGraphique() < 0)
  {
    printf("Erreur de OuvrirGrilleSDL\n");
    fflush(stdout);
    exit(1);
  }

  DessineGrilleBase();
  pthread_create(&tidPacMan,NULL,ThreadPacMan,NULL);
  pthread_create(&tidEvent,NULL,ThreadEvent,NULL);
  
  
  pthread_join(tidEvent,NULL);
  
  printf("Attente de 1500 millisecondes...\n");
  Attente(1500);

  
  
  
  // -------------------------------------------------------------------------
  
  // Fermeture de la fenetre
  printf("(MAIN %p) Fermeture de la fenetre graphique...",pthread_self()); fflush(stdout);
  FermetureFenetreGraphique();
  printf("OK\n"); fflush(stdout);

  printf("Thread PacMan terminé\n");
  exit(0);
}

//*********************************************************************************************
void Attente(int milli) {
  struct timespec del;
  del.tv_sec = milli/1000;
  del.tv_nsec = (milli%1000)*1000000;
  nanosleep(&del,NULL);
}

//*********************************************************************************************
void setTab(int l, int c, int presence, pthread_t tid) {
  tab[l][c].presence = presence;
  tab[l][c].tid = tid;
}

//*********************************************************************************************
void DessineGrilleBase() {
  int t[NB_LIGNE][NB_COLONNE]
    = { {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1},
        {1,0,1,1,0,1,1,0,1,0,1,1,0,1,1,0,1},
        {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,0,1,1,0,1,0,1,1,1,0,1,0,1,1,0,1},
        {1,0,0,0,0,1,0,0,1,0,0,1,0,0,0,0,1},
        {1,1,1,1,0,1,1,0,1,0,1,1,0,1,1,1,1},
        {1,1,1,1,0,1,0,0,0,0,0,1,0,1,1,1,1},
        {1,1,1,1,0,1,0,1,0,1,0,1,0,1,1,1,1},
        {0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,0,0},
        {1,1,1,1,0,1,0,1,1,1,0,1,0,1,1,1,1},
        {1,1,1,1,0,1,0,0,0,0,0,1,0,1,1,1,1},
        {1,1,1,1,0,1,0,1,1,1,0,1,0,1,1,1,1},
        {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1},
        {1,0,1,1,0,1,1,0,1,0,1,1,0,1,1,0,1},
        {1,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,1},
        {1,1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,1},
        {1,0,0,0,0,1,0,0,1,0,0,1,0,0,0,0,1},
        {1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,0,1},
        {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}};

  for (int l=0 ; l<NB_LIGNE ; l++)
    for (int c=0 ; c<NB_COLONNE ; c++) {
      if (t[l][c] == VIDE) {
        setTab(l,c);
        EffaceCarre(l,c);
      }
      if (t[l][c] == MUR) {
        setTab(l,c,MUR); 
        DessineMur(l,c);
      }
    }
}

//*********************************************************************************************

void* ThreadPacMan(void* p)
{
  //on arme les signaux pour les déplacements
  struct sigaction A, B, C, D;
  sigset_t mask;
  A.sa_handler = handler_SIGINT;
  A.sa_flags = 0;
  sigemptyset(&A.sa_mask);
  sigaction(SIGINT, &A, NULL);
  
  B.sa_handler = handler_SIGINT;
  B.sa_flags = 0;
  sigemptyset(&B.sa_mask);
  B.sa_handler = handler_SIGHUP;
  sigaction(SIGHUP, &B, NULL);
  
  C.sa_handler = handler_SIGINT;
  C.sa_flags = 0;
  sigemptyset(&C.sa_mask);
  C.sa_handler = handler_SIGUSR1;
  sigaction(SIGUSR1, &C, NULL);
  
  D.sa_handler = handler_SIGINT;
  D.sa_flags = 0;
  sigemptyset(&D.sa_mask);
  D.sa_handler = handler_SIGUSR2;
  sigaction(SIGUSR2, &D, NULL);

  //on preparer le vecteur de signaux a bloquer
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  int l = LENTREE, c = CENTREE, newL, newC;

  while (1)
  {
    //section critique direction et tableau tab
    pthread_mutex_lock(&mutexTab);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    setTab(l, c);
    EffaceCarre(l, c);

    //on verifie la nouvelle position avec la présence d'un mur
    newL = l;
    newC = c;

    if (dir == GAUCHE) newC--;
    else if (dir == DROITE) newC++;
    else if (dir == HAUT) newL--;
    else if (dir == BAS) newL++;
    
    if (tab[newL][newC].presence != MUR)
    { 
      l = newL;
      c = newC;
    } 
    
    setTab(l, c, PACMAN, pthread_self());
    pthread_mutex_unlock(&mutexTab);
    pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

    DessinePacMan(l, c, dir);
    
    //section critique Attente
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
    printf("Attente de 300 millisecondes...\n");
    Attente(300);
    pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

  }
}
void* ThreadEvent(void* p)
{
  EVENT_GRILLE_SDL event;

  int ok;
  ok = 0;
  while(!ok)
  {
    event = ReadEvent();
    if (event.type == CROIX) ok = 1;
    if (event.type == CLAVIER)
    {
      switch(event.touche)
      {
        case 'q' : ok = 1; break;
        case KEY_RIGHT : 
                        printf("Fleche droite !\n");
                        pthread_kill(tidPacMan, SIGHUP);
        break;
        case KEY_LEFT : 
                        printf("Fleche gauche !\n");
                        pthread_kill(tidPacMan, SIGINT);
        break;
        case KEY_UP : 
                        printf("Fleche haut !\n");
                        pthread_kill(tidPacMan, SIGUSR1);
        break;
        case KEY_DOWN :
                        printf("Fleche bas !\n");
                        pthread_kill(tidPacMan, SIGUSR2);
        break;

      }
    }
  }

  pthread_exit(0);
}

void handler_SIGINT(int sig) { dir = GAUCHE; }
void handler_SIGHUP(int sig) { dir = DROITE; }
void handler_SIGUSR1(int sig) { dir = HAUT; }
void handler_SIGUSR2(int sig) { dir = BAS; }