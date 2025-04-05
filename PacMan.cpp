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

//variables Globales
int dir = GAUCHE, nbPacGom = 0, niveau = 1, delai = 300, score = 0, vies = 3;
S_CASE tab[NB_LIGNE][NB_COLONNE];
bool MAJScore = false;
int nbRouge = 0, nbVert = 0, nbMauve = 0, nbOrange = 0, mode = 1;
bool TimeOutRunning = false;
//Threads,mutex et variables de condition
pthread_mutex_t mutexDelai = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexTab = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexNbPacGom = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexScore = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexNbFantomes = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexVies = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexMode = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexDir = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condNbFantomes = PTHREAD_COND_INITIALIZER;
pthread_cond_t condNbPacGom = PTHREAD_COND_INITIALIZER;
pthread_cond_t condScore = PTHREAD_COND_INITIALIZER;
pthread_cond_t condVies = PTHREAD_COND_INITIALIZER;
pthread_cond_t condMode = PTHREAD_COND_INITIALIZER;

pthread_t tidPacMan, tidEvent, tidPacGom,
tidScore, tidBonus, tidCompteur, tidVies, tidTimeOut;
void* ThreadPacMan(void* p);
void* ThreadEvent(void* p);
void* ThreadPacGom(void* p);
void* ThreadScore(void* p);
void* ThreadBonus(void* p);
void* ThreadCompteurFantome(void* p);
void* ThreadFantome(void* p);
void* ThreadVies(void* p);
void* ThreadTimeOut(void* p);

pthread_key_t keyFantome;
//Fonctions
void DessineGrilleBase();
void Attente(int milli);
void setTab(int l, int c, int presence = VIDE, pthread_t tid = 0);
void MangerSuperPacGom();
void cleanupFantome(void* arg);

//Handlers de signaux
void handler_SIGINT(int sig);
void handler_SIGHUP(int sig);
void handler_SIGUSR1(int sig);
void handler_SIGUSR2(int sig);
void handler_SIGCHLD(int sig);
void handler_SIGALRM(int sig);
void handler_SIGQUIT(int sig);
///////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc,char* argv[])
{
  sigset_t mask;
  struct sigaction sigAct;
  char ok;
  sigAct.sa_handler = handler_SIGINT;
  sigAct.sa_flags = 0;
  sigemptyset(&sigAct.sa_mask);
  sigaction(SIGINT, &sigAct, NULL);
  
  sigAct.sa_flags = 0;
  sigemptyset(&sigAct.sa_mask);
  sigAct.sa_handler = handler_SIGHUP;
  sigaction(SIGHUP, &sigAct, NULL);
  
  sigAct.sa_flags = 0;
  sigemptyset(&sigAct.sa_mask);
  sigAct.sa_handler = handler_SIGUSR1;
  sigaction(SIGUSR1, &sigAct, NULL);
  
  sigAct.sa_flags = 0;
  sigemptyset(&sigAct.sa_mask);
  sigAct.sa_handler = handler_SIGUSR2;
  sigaction(SIGUSR2, &sigAct, NULL);

  sigAct.sa_flags = 0;
  sigemptyset(&sigAct.sa_mask);
  sigaddset(&mask,SIGCHLD);
  sigAct.sa_handler = handler_SIGCHLD;
  sigaction(SIGCHLD, &sigAct, NULL);

  sigAct.sa_flags = 0;
  sigemptyset(&sigAct.sa_mask);
  sigaddset(&mask, SIGALRM);
  sigAct.sa_handler = handler_SIGALRM;
  sigaction(SIGALRM, &sigAct, NULL);

  sigAct.sa_flags = 0;
  sigemptyset(&sigAct.sa_mask);
  sigaddset(&mask, SIGQUIT);
  sigAct.sa_handler = handler_SIGQUIT;
  sigaction(SIGQUIT, &sigAct, NULL);
  sigprocmask(SIG_BLOCK, &mask, NULL);
  
  pthread_key_create(&keyFantome,NULL);

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

  pthread_create(&tidVies,NULL,ThreadVies,NULL);
  pthread_create(&tidPacGom,NULL,ThreadPacGom,NULL);
  pthread_create(&tidEvent,NULL,ThreadEvent,NULL);
  pthread_create(&tidScore,NULL,ThreadScore,NULL);
  pthread_create(&tidBonus,NULL,ThreadBonus,NULL);
  pthread_create(&tidCompteur,NULL,ThreadCompteurFantome,NULL);

  
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

void MangerSuperPacGom()
{
  pthread_mutex_lock(&mutexScore);
  pthread_mutex_lock(&mutexNbPacGom);
  score += 5;
  nbPacGom--;
  pthread_mutex_unlock(&mutexNbPacGom);
  pthread_mutex_unlock(&mutexScore);
  pthread_cond_signal(&condNbPacGom);
  pthread_cond_signal(&condScore);
  
  pthread_mutex_lock(&mutexMode);
  mode = 2;
  pthread_mutex_unlock(&mutexMode);
  
  int* tempsRestant = (int*)malloc(sizeof(int));
  

  *tempsRestant = 0;  

  if(TimeOutRunning)
  {
    *tempsRestant = alarm(0); // Sauvegarde du temps restant
    printf("temps restant a ajouter au timeout: %d seconde\n", *tempsRestant);
    pthread_kill(tidTimeOut, SIGQUIT);
  }

  pthread_create(&tidTimeOut, NULL, ThreadTimeOut, (void*)tempsRestant);
  printf("Le nouveau timeout a ete cree avec %d secondes restantes\n", *tempsRestant);
  
  
}

//*********************************************************************************************

void* ThreadPacMan(void* p)
{
  //on arme les signaux pour les déplacements
  sigset_t mask;

  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  int l = LENTREE, c = CENTREE, newL, newC;

  // Routine de nettoyage pour déverrouiller les mutex
  auto cleanup = [](void* arg) {
    pthread_mutex_unlock(&mutexTab);
    pthread_mutex_unlock(&mutexDelai);
  };

  pthread_cleanup_push(cleanup, NULL);


  while (1)
  {
    //section critique direction et tableau tab
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
    pthread_mutex_lock(&mutexTab);
    setTab(l, c);
    EffaceCarre(l, c);


    
    //on verifie la nouvelle position avec la présence d'un mur
    newL = l;
    newC = c;

    if (dir == GAUCHE) newC--;
    else if (dir == DROITE) newC++;
    else if (dir == HAUT) newL--;
    else if (dir == BAS) newL++;
    
    if(l == 9 && c == 0 && dir == GAUCHE)
    {
      newL = 9; 
      newC = 16;
    }
    else if(l == 9 && c == 16 && dir == DROITE)
    {
      newL = 9; 
      newC = 0;
    }
    if (tab[newL][newC].presence != MUR)
    { 
      l = newL;
      c = newC;

      switch(tab[l][c].presence)
      {
        case PACGOM:
                    pthread_mutex_lock(&mutexScore);
                    pthread_mutex_lock(&mutexNbPacGom);
                    score++;
                    nbPacGom--;
                    printf("Score: %d\n",score);
                    printf("nb PacGom: %d\n",nbPacGom);
                    pthread_mutex_unlock(&mutexNbPacGom);
                    pthread_mutex_unlock(&mutexScore);
                    pthread_cond_signal(&condNbPacGom);
                    pthread_cond_signal(&condScore);
                  break;
        case SUPERPACGOM:
                    MangerSuperPacGom();
                  break;
        case BONUS:
                    pthread_mutex_lock(&mutexScore);
                    score = score + 30;
                    printf("Score: %d\n",score);
                    pthread_mutex_unlock(&mutexScore);
                    pthread_cond_signal(&condScore);
                  break;
        case FANTOME:
                    if(mode == 1)
                    {
                      setTab(l, c, VIDE, 0);   
                      EffaceCarre(l, c); 
                      pthread_mutex_unlock(&mutexTab);
                      pthread_exit(0);
                    }
                    else if(mode == 2)
                    {
                      pthread_kill(tab[l][c].tid, SIGCHLD);
                    }
                  break;
      }
      
    }
    setTab(l, c, PACMAN, pthread_self());
    pthread_mutex_unlock(&mutexTab);
    pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

    DessinePacMan(l, c, dir);
    
    //section critique Attente et variable global delai
    pthread_mutex_lock(&mutexDelai);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
    Attente(delai);
    pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
    pthread_mutex_unlock(&mutexDelai);
  }

  pthread_cleanup_pop(0);
  pthread_exit(0);
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
void* ThreadPacGom(void* p)
{

  do
  {
    pthread_mutex_lock(&mutexTab);
    
    DessineChiffre(14,22,niveau);
    
    for(int i = 0; i < NB_LIGNE; i++)
    {
      for(int j = 0; j < NB_COLONNE; j++)
      {
        if(tab[i][j].presence == VIDE || tab[i][j].presence == FANTOME)
        {
          if((i == LENTREE && j == CENTREE) || (i == 8 && j == 8) || (i == 9 && j == 8));
          else
          {
            setTab(i,j,PACGOM);

            DessinePacGom(i, j);
            pthread_mutex_lock(&mutexNbPacGom);
            nbPacGom++;
            pthread_mutex_unlock(&mutexNbPacGom);

          }
        }
      }
    }
    
    pthread_mutex_unlock(&mutexTab);

    printf("nb PacGom: %d\n",nbPacGom);
    DessineChiffre(12,22,nbPacGom/100);
    DessineChiffre(12,23,(nbPacGom/10)%10);
    DessineChiffre(12,24,nbPacGom%10);

    pthread_mutex_lock(&mutexTab);
    setTab(2,1,SUPERPACGOM);
    setTab(2,15,SUPERPACGOM);
    setTab(15,1,SUPERPACGOM);
    setTab(15,15,SUPERPACGOM);
    pthread_mutex_unlock(&mutexTab);

    DessineSuperPacGom(2,1);
    DessineSuperPacGom(2,15);
    DessineSuperPacGom(15,1);
    DessineSuperPacGom(15,15);
    
    //attente du signal de pacman pour afficher le nbPacGom restant
    pthread_mutex_lock(&mutexNbPacGom);
    while(nbPacGom)
    {
      pthread_cond_wait(&condNbPacGom,&mutexNbPacGom);
      DessineChiffre(12,22,nbPacGom/100);
      DessineChiffre(12,23,(nbPacGom/10)%10);
      DessineChiffre(12,24,nbPacGom%10);

    }
    pthread_mutex_unlock(&mutexNbPacGom);

    //on incrémente le niveau de difficulté
    niveau++;
    DessineChiffre(14,22,niveau);


    for(int i = 0; i < NB_LIGNE; i++)
    {
      for(int j = 0; j < NB_COLONNE; j++)
      {
        if(tab[i][j].presence == FANTOME)
        {
          pthread_kill(tab[i][j].tid, SIGQUIT);
          pthread_mutex_lock(&mutexTab);
          setTab(i,j);
          EffaceCarre(i,j);
          pthread_mutex_unlock(&mutexTab);
        }

      }
    }
    pthread_mutex_lock(&mutexMode);
    mode = 1;
    pthread_mutex_unlock(&mutexMode);

    pthread_mutex_lock(&mutexDelai);
    delai = delai/2;
    pthread_mutex_unlock(&mutexDelai);

  } while (niveau);
  pthread_exit(0);
  
}

void* ThreadScore(void* p)
{
  //affichage du score 0 au lancement
  DessineChiffre(16,22,score/1000);
  DessineChiffre(16,23,(score/100)%10);
  DessineChiffre(16,24,(score/10)%10);
  DessineChiffre(16,25,score%10);
  
  
  //attente du signal de pacman pour afficher le nouveau score
  pthread_mutex_lock(&mutexScore);
  while(MAJScore == false)
  {
    pthread_cond_wait(&condScore,&mutexScore);
    DessineChiffre(16,22,score/1000);
    DessineChiffre(16,23,(score/100)%10);
    DessineChiffre(16,24,(score/10)%10);
    DessineChiffre(16,25,score%10);
    MAJScore = false;
  }
  pthread_mutex_unlock(&mutexScore);
  
  pthread_exit(0);

}

void* ThreadBonus(void* p)
{
  int tempsAttente, i, j;
  while(1)
  {
    //on génère un int random entre 10 et 20
    tempsAttente = (rand() % 11 + 10) * 1000;
    Attente(tempsAttente);
    
    //on place le bonus sur une case vide de maniere random
    if(tab[i = rand()%22][j = rand()%18].presence == 0)
    {
      pthread_mutex_lock(&mutexTab);
      setTab(i,j,BONUS);
      pthread_mutex_unlock(&mutexTab);
      DessineBonus(i, j);
      Attente(10000);

      if(tab[i][j].presence == BONUS)
      {
        pthread_mutex_lock(&mutexTab);
        setTab(i,j,VIDE);
        EffaceCarre(i,j);
        pthread_mutex_unlock(&mutexTab);

      }
    }
  
  }
  pthread_exit(0);

}

void* ThreadCompteurFantome(void* p)
{ 
  while(1)
  {
    pthread_mutex_lock(&mutexNbFantomes);
    
    while (nbRouge == 2 && nbVert == 2 && nbMauve == 2 && nbOrange == 2) {
      
      pthread_cond_wait(&condNbFantomes, &mutexNbFantomes);

    }
    pthread_mutex_lock(&mutexMode);
    while(mode == 2)pthread_cond_wait(&condMode, &mutexMode);
    pthread_mutex_unlock(&mutexMode);
    
    S_FANTOME* fantome = (S_FANTOME*)malloc(sizeof(S_FANTOME));
    fantome->L = 9;
    fantome->C = 8;
    fantome->cache = VIDE;

    if (nbRouge < 2) { fantome->couleur = ROUGE; nbRouge++; }
    else if (nbVert < 2) { fantome->couleur = VERT; nbVert++; }
    else if (nbMauve < 2) { fantome->couleur = MAUVE; nbMauve++; }
    else if (nbOrange < 2) { fantome->couleur = ORANGE; nbOrange++; }
    pthread_mutex_unlock(&mutexNbFantomes);
    pthread_t tidFantome;
    pthread_create(&tidFantome, NULL, ThreadFantome, (void*)fantome);
    printf("Création du fantôme de couleur %d\n", fantome->couleur);
    
    
   
    
   
    
  }
    
  
  pthread_exit(0);
}


void* ThreadFantome(void* p)
{ 

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGQUIT);
  sigaddset(&mask, SIGCHLD);

  pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

  S_FANTOME* fantome = (S_FANTOME*) p;

  // Associer la structure fantôme à la clé spécifique du thread
  pthread_setspecific(keyFantome, (void*)fantome);
  printf("ligne = %d et colone du fantome de couleur : %d\n",fantome->L,fantome->C,fantome->couleur);
  
  pthread_cleanup_push(cleanupFantome,NULL);
  int dirFantome = HAUT;
  if(tab[fantome->L][fantome->C].presence == VIDE)
  {
    pthread_mutex_lock(&mutexTab);
    setTab(fantome->L, fantome->C, FANTOME, pthread_self());
    DessineFantome(fantome->L, fantome->C, fantome->couleur, dirFantome);
    pthread_mutex_unlock(&mutexTab);

  }
  
  

  while (1)
  {
    // Logique de déplacement
    int newL = fantome->L;
    int newC = fantome->C;

    if (dirFantome == HAUT) newL--;
    else if (dirFantome == BAS) newL++;
    else if (dirFantome == GAUCHE) newC--;
    else if (dirFantome == DROITE) newC++;

    pthread_mutex_lock(&mutexTab);

    if(tab[newL][newC].presence == PACMAN && mode == 1)
    {

      pthread_cancel(tab[newL][newC].tid);
      printf("PACMAN a été tué !!\n");
      tab[newL][newC].presence = VIDE;
      EffaceCarre(newL,newC);

      
    }
    else if(tab[newL][newC].presence == PACMAN && mode == 2)
    {
      switch(dirFantome)
      {
        case HAUT: newL++;
        break;
        case BAS: newL--;
        break;
        case GAUCHE: newC++;
        break;
        case DROITE: newC--;
      }
    }
    pthread_mutex_unlock(&mutexTab);

    pthread_mutex_lock(&mutexTab);
    
    if (tab[newL][newC].presence != MUR && tab[newL][newC].presence != FANTOME)
    {
      

      setTab(fantome->L, fantome->C, fantome->cache, 0);
      EffaceCarre(fantome->L,fantome->C);
      

      switch(fantome->cache)
      {
        case PACGOM: DessinePacGom(fantome->L,fantome->C); break;
        case SUPERPACGOM: DessineSuperPacGom(fantome->L,fantome->C); break;
        case BONUS: DessineBonus(fantome->L,fantome->C); break;
      }
      
      fantome->cache = tab[newL][newC].presence;
      fantome->L = newL;
      fantome->C = newC;
      
      setTab(fantome->L, fantome->C, FANTOME, pthread_self());
      if(mode ==1) DessineFantome(fantome->L, fantome->C, fantome->couleur, dirFantome);
      else if(mode == 2) DessineFantomeComestible(fantome->L, fantome->C);
    }
    else if(vies == 0)
    {
      pthread_mutex_lock(&mutexVies);
      while(vies == 0) pthread_cond_wait(&condVies,&mutexVies);
      pthread_mutex_unlock(&mutexVies);
    
    }
    else 
    {
      int dirs[4] = {HAUT, BAS, GAUCHE, DROITE};
      int dirsPossible[4];
      int count = 0;

      for (int i = 0; i < 4; i++)
      {
        int testL = fantome->L, testC = fantome->C;
        switch (dirs[i])
        {
          case HAUT: testL--; break;
          case BAS: testL++; break;
          case GAUCHE: testC--; break;
          case DROITE: testC++; break;
        }
        if (tab[testL][testC].presence != MUR && tab[testL][testC].presence != FANTOME)
        {
          dirsPossible[count++] = dirs[i];
        }
      }

      if (count > 0) dirFantome = dirsPossible[rand() % count]; // Choisit une direction aléatoire valide
      
      
    
    }
    pthread_mutex_unlock(&mutexTab);

    // Attente avant le prochain déplacement

    Attente((5 * delai) / 3);

  }
  pthread_cleanup_pop(1);
  pthread_exit(0);
}

void* ThreadVies(void* p)
{
  
  
  while(vies > 0)
  {
    pthread_mutex_lock(&mutexVies);
    DessineChiffre(18,22, vies);
    pthread_create(&tidPacMan,NULL,ThreadPacMan,NULL);
    pthread_join(tidPacMan, NULL);
    vies--;
    DessineChiffre(18,22,vies);
    pthread_mutex_unlock(&mutexVies);

  }
  

  DessineChiffre(18,22,vies);
  DessineGameOver(9,4);
  
  pthread_mutex_lock(&mutexVies);
  pthread_mutex_unlock(&mutexVies);
  pthread_cond_signal(&condVies);
  pthread_exit(0);

}

void* ThreadTimeOut(void* p)
{
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);
  sigaddset(&mask, SIGQUIT);

  pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
  printf("signal SIGALRM débloqué\n");
  
  TimeOutRunning = true;
  int tempsRestant = *(int*)p;
  
  int tempsAleatoire = (rand() % 8 + 8);
  printf("(ThreadTimeOut) temps aléatoire : %d\n", tempsAleatoire);
  
  tempsAleatoire = tempsAleatoire + tempsRestant;
  printf("(ThreadTimeOut) temps aléatoire + temps restant : %d\n", tempsAleatoire);
  
  free(p);  

  
  alarm(tempsAleatoire);
  printf("alarm fait normalement\n");
  pause();

  printf("signal SIGALRM reçu\n");
  TimeOutRunning = false;
    
  pthread_exit(0);
}
//*********************************************************************************************

//handlers des signaux
void handler_SIGINT(int sig)
{
  pthread_mutex_lock(&mutexDir);
  dir = GAUCHE;
  pthread_mutex_unlock(&mutexDir);

}
void handler_SIGHUP(int sig)
{
  pthread_mutex_lock(&mutexDir);
  dir = DROITE;
  pthread_mutex_unlock(&mutexDir);

}
void handler_SIGUSR1(int sig)
{
  pthread_mutex_lock(&mutexDir);
  dir = HAUT;
  pthread_mutex_unlock(&mutexDir);

}
void handler_SIGUSR2(int sig)
{
  pthread_mutex_lock(&mutexDir);
  dir = BAS;
  pthread_mutex_unlock(&mutexDir);

}
void handler_SIGCHLD(int sig)
{ 
  pthread_exit(NULL);
}

void handler_SIGALRM(int sig)
{
  printf("SIGALRM reçu dans le handler\n");

  pthread_mutex_lock(&mutexMode);
  mode = 1;
  printf("mode repasse a 1\n");
  pthread_mutex_unlock(&mutexMode);
  pthread_cond_signal(&condMode);

}

void handler_SIGQUIT(int sig)
{
  pthread_exit(NULL);
}

void cleanupFantome(void* arg)
{
  S_FANTOME* fantome = (S_FANTOME*)pthread_getspecific(keyFantome);
  if (!fantome) return;

  if(mode == 2)
  {
    pthread_mutex_lock(&mutexScore);
    score += 50 + fantome->cache;
    MAJScore = true;
    pthread_mutex_unlock(&mutexScore);
    pthread_cond_signal(&condScore);

  }
  

  if (fantome->cache == PACGOM || fantome->cache == SUPERPACGOM) {
      pthread_mutex_lock(&mutexNbPacGom);
      nbPacGom--;
      pthread_mutex_unlock(&mutexNbPacGom);
      pthread_cond_signal(&condNbPacGom);
  }

  pthread_mutex_lock(&mutexNbFantomes);
  switch (fantome->couleur) {
      case ROUGE: nbRouge--; break;
      case VERT: nbVert--; break;
      case MAUVE: nbMauve--; break;
      case ORANGE: nbOrange--; break;
  }
  pthread_mutex_unlock(&mutexNbFantomes);
  pthread_cond_signal(&condNbFantomes);

  free(fantome);
}