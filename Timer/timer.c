#include <stdio.h>     //librerie   
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>  
#include <time.h>


#define MAX_EXAMS 8    //massimo numero di esami che possono essere visualizzati
#define MAX_LINE 160   //massimo numero di caratteri in una riga del file di configurazione
#define MAX_NAME 140   //massimo numero di caratteri per il nome di un esame


struct exam {
   char name[MAX_NAME];   //nome del'esame
   int minutes;           //durata in minuti
   GtkLabel *nameLabel;   //casella di testo corrispondente al nome dell'esame nell'interfaccia grafica
   GtkLabel *timeLabel;   //casella di testo corrispondente al tempo rimanente nell'interfaccia grafica
   int blink;             //serve a far lampeggiare il testo "CONSEGNA"
};


struct exam exams[MAX_EXAMS];   //vettore che contiene tutti gli esami validi trovati
int examsCount = 0;             //numero di esami validi trovati
int width = 1920;               //larghezza dello schermo in pixel
int height = 1080;              //altezza dello schermo in pixel
time_t start;                   //ora di inizio degli esami
gboolean isStarted = FALSE;     //tiene conto del fatto che gli esami siano iniziati
PangoAttrList *attrlistName;    //attributi del testo
PangoAttrList *attrlistTime;
gboolean isConnected = FALSE;   //segnala se la chiavetta USB è collegata


void setAttributes(void);             //prototipi delle funzioni
void createStartInterface(void);
gboolean readFile(void);               
void createInterface(void);
gboolean set_exam(gpointer data); 
void key_pressed(GtkWindow *window);


int main(void){             //main

   setAttributes();

   createStartInterface();

   createInterface();

   return 0;
  
}


void setAttributes(void){  //imposta gli attributi del testo e rileva la risoluzione dello schermo
  
   int nameSize;          
   int timeSize;
   GdkScreen *screen;	
   gtk_init(NULL, NULL);
  
   if((screen = gdk_screen_get_default()) != NULL){   //rilevo l'altezza e la larghezza dello schermo in pixel
	  width = gdk_screen_get_width(screen);
	  height = gdk_screen_get_height(screen);
   }
	
   nameSize = (25000*height)/1080;  //l'interfaccia è stata tarata su uno schermo FullHD
   timeSize = (40000*height)/1080;  //viene fatta la proporzione per adattarla ad ogni schermo
 
   PangoAttribute *attrName;                         //utilizzo un font più piccolo per il nome degli esami
   attrlistName = pango_attr_list_new();             
   attrName = pango_attr_size_new (nameSize);
   pango_attr_list_insert(attrlistName, attrName);

   PangoAttribute *attrTime;                        //ed uno più grande per il tempo rimasto e i messaggi di errore
   attrlistTime = pango_attr_list_new();
   attrTime = pango_attr_size_new (timeSize);
   pango_attr_list_insert(attrlistTime, attrTime);
  
}


void createStartInterface(void){  //visualizza l'interfaccia iniziale

   GtkWindow *window;
   GtkLabel *startLabel;  
  
   gtk_init(NULL, NULL);  //inizializzo gtk
  
   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);  //creo una nuova finestra
   gtk_window_fullscreen (window);                //faccio in modo che sia visualizzata a pieno schermo
   g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  
   startLabel = gtk_label_new("Ricerca di dispositivi USB...");   //creo una nuova casella di testo
   gtk_label_set_attributes(startLabel, attrlistTime);            //imposto gli attributi del testo
   gtk_label_set_ellipsize(startLabel, PANGO_ELLIPSIZE_END);      //accorcia il testo con dei punti 
                                                                  //qualora non dovesse esserci abbastanza spazio
   gtk_label_set_xalign(startLabel, 0.5);                         //centro il testo
   gtk_container_add(GTK_CONTAINER(window), startLabel);          //inserisco la casella di testo nella finestra
  
   gtk_widget_show_all(window);                                   //visualizzo la finestra
  
   g_timeout_add(2000,readFile, NULL);  //chiamo la funzione che legge il file con la lista degli esami dopo 2s
  
   gtk_main();   //inizio il main loop di gtk
  
}


gboolean readFile(void){  //legge il file con la lista degli esami

   char line[MAX_LINE];
   char *name;
   char *hours;
   char *minutes;
   int time = 0;
   FILE *config;
   int i = 0;
  
   char *user = getenv("USER");  //legge il nome utente dal sistema
   char path[150];               //il percorso della chiavetta USB
   struct dirent *de;
   int attempts = 0;             //numero di tentativi di trovare un dispositivo USB
   
   strcpy(path, "/media/");
   strcat(path, user);
   DIR *dr = opendir(path);      //apre la cartella

   while ((attempts < 80) && !(isConnected)){  //se non è connessa una chiavetta USB e se non sono stati fatti più di 80 tentativi (40s)
	   
      if ((de = readdir(dr)) == NULL){
         rewinddir(dr);    //riparte dall'inizio della cartella
         attempts++;
         usleep(500000);   //attende 500ms prima di riprovare
      }else if ((de->d_name[0] != '.')){        //se la cartella non è vuota
         isConnected = TRUE;
      }

   }

   if ((attempts < 80) && (de != NULL)){
      strcat(path, "/");
      strcat(path, de->d_name);
      strcat(path, "/esami.txt");  //ricostruisco il percorso completo del file con la lista degli esami

      if((config = fopen(path, "r")) == NULL){  //se non riesco ad aprire il file
         examsCount = -1;  //non è stato trovato il file con la lista degli esami
	  }else{

	     while((!feof(config)) && (examsCount < MAX_EXAMS)){  //leggo il file fino a che non arrivo alla fine o al numero massimo di esami supportati

            fgets(line,MAX_LINE,config);     //leggo una riga del file

            i = 0;
            while(line[i] == ' '){  //salto gli spazi all'inizio della riga
               i++;
            }
      
            if(line[i] == '/' && line[i+1] == '/'){  //se trovo i simboli // ignoro la riga
               time = 0;
            }else{
			   name = strtok(line,"=");         //il nome dell'esame è a sinistra del simbolo =
		       hours = strtok(NULL,".,:\n\r");  //le ore sono a sinistra del simbolo . oppure : oppure a capo
		       minutes = strtok(NULL,"\n\r");   //i minuti sono a sinistra del simbolo a capo
		       if((hours != NULL) && (minutes != NULL)){          //converto le stringhe minutes e hours in un intero che rappresenta i minuti di durata
	              time = 60 * atoi(hours) + atoi(minutes);
		       }else if((hours != NULL) && (minutes == NULL)){
		          time = 60 * atoi(hours);
		       }else{
                  time = 0;
		       }
            }

            if(time != 0){
				
               for (i=0; i<strlen(name); i++){  
                  name[i] = toupper(name[i]);          //scrivo il nome in maiuscolo
               }
			   
               stpcpy(exams[examsCount].name, name);  //popolo il vettore di struct exam con 
               exams[examsCount].minutes = time;      //le informazioni lette dal file
               exams[examsCount].blink = 0;           //inizializzo blink a 0
               examsCount++;

            }

	     }
		 
         fclose(config);  //chiudo il file
	  }
   }else{
      examsCount = -2;  //non è stata trovata una chiavetta USB
   }
  
   closedir(dr);     //chiudo la cartella
  
   gtk_main_quit();  //esco dal main loop di gtk
  
   return FALSE;     //la funzione non verrà più chiamata

}


void createInterface(void){   //visualizza l'interfaccia principale

   GtkWindow *window;
   GtkWidget *grid;

   int e = 0;
  
   int margin;
   margin = (80*width)/1920;  //definisco il margine e lo scalo in base alla larghezza dello schermo

   gtk_init(NULL, NULL);   //inizializzo gtk

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);   //creo una nuova finestra
   gtk_window_fullscreen (window);                 //faccio in modo che sia visualizzata a pieno schermo
   g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

   grid = gtk_grid_new ();                           //creo una nuova griglia
   gtk_grid_set_row_homogeneous (grid, TRUE);        //le righe devono avere tutte la stassa altezza 
   gtk_grid_set_column_homogeneous (grid, TRUE);     //le colonne devono avere tutte la stassa larghezza
   gtk_widget_set_margin_end(grid, margin);          //imposto i margini destro e sinistro
   gtk_widget_set_margin_start(grid, margin);        
   gtk_container_add(GTK_CONTAINER(window), grid);   //inserisco la griglia nella finestra

   if (examsCount > 0){  //se è stato trovato almeno un esame valido

      for (e = 0; e < examsCount; e++){  //scorro il vettore degli esami

         exams[e].nameLabel = gtk_label_new(exams[e].name);   //creo i campi di testo per il mome e il tempo rimasto
         exams[e].timeLabel = gtk_label_new(NULL);
 
         gtk_label_set_ellipsize(exams[e].nameLabel, PANGO_ELLIPSIZE_END);   //accorcia il testo con dei punti
         gtk_label_set_ellipsize(exams[e].timeLabel, PANGO_ELLIPSIZE_END);   //qualora non dovesse esserci abbastanza spazio


         gtk_label_set_xalign(exams[e].nameLabel, 0);  //allineo il nome a sinistra
         gtk_label_set_xalign(exams[e].timeLabel, 1);  //allineo il tempo rimanente a destra

         gtk_label_set_attributes(exams[e].nameLabel, attrlistName);  //imposto gli attributi del testo
         gtk_label_set_attributes(exams[e].timeLabel, attrlistTime);

	     gtk_grid_attach (grid, exams[e].nameLabel, 1, e, 3, 1);  //inserisco la casella di testo del nome nella griglia alla colonna 1,
	                                                              //riga e, con larghezza 3 colonne e altezza 1 													   
         gtk_grid_attach (grid, exams[e].timeLabel, 4, e, 1, 1);  //inserisco la casella di testo del tempo rimanente alla colonna 4, riga e

         set_exam(e);  //chiamo la funzione che riempie le caselle di testo e le passo il numero dell'esame

      }

   }else{  //se non è stato trovato alcun esame valido

      GtkLabel *errorLabel;  //creo una casella di testo

      if(examsCount == 0){  //capisco il tipo di problema riscontrato
         errorLabel = gtk_label_new("Non è stato trovato alcun esame valido");
      }else if(examsCount == -1){
         errorLabel = gtk_label_new("Non è stato trovato alcun file di configurazione");
      }else if(examsCount == -2){
         errorLabel = gtk_label_new("Non è stato trovato alcun dispositivo USB");
      }
	  
      gtk_label_set_xalign(errorLabel, 0.5);                      //centro il testo
      gtk_label_set_ellipsize(errorLabel, PANGO_ELLIPSIZE_END);   //accorcia il testo con dei punti, qualora non dovesse esserci abbastanza spazio
      gtk_label_set_attributes(errorLabel, attrlistTime);         //imposto gli attributi del testo
      gtk_grid_attach (grid, errorLabel, 1, 1, 1, 1);             //inserisco la casella di testo con l'errore nella griglia
   }

   g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (key_pressed), NULL);  //chiamo la funzione che esegue le operazioni necessarie
                                                                                            //quando viene rilevata la pressione di un pulsante
   gtk_widget_show_all(window);  //visualizzo la finestra

   gtk_main();  //inizio il main loop di gtk

}


gboolean set_exam(gpointer data){  //aggiorna le caselle di testo corrispondenti agli esami

   int seconds;
   char string [16];
   int h;
   int m;
   int s;
   time_t now;

   int i = (int*)data; 

   time(&now);  //acquisisco l'ora attuale

   seconds = exams[i].minutes * 60;  //converto i minuti in secondi
  
   if (isStarted == TRUE){               //se gli esami sono iniziati
      seconds -= difftime (now,start);    //faccio la differenza tra l'ora di partenza e quella attuale
   }
  
   h = (seconds/60)/60;  //conversione in ore, minuti e secondi
   m = (seconds/60)%60;
   s = (seconds%60)%60;

   if ((h<=0) && (m<=0) && (s<=0)){  //se l'esame è finito
      if(m > -2){  //se l'esame è finito da meno di 2 minuti
         if(exams[i].blink < 7){  
            gtk_label_set_label(exams[i].timeLabel, "CONSEGNA");  //la scritta CONSEGNA lampeggia
            exams[i].blink++;
	     }else if(exams[i].blink < 10){
		    gtk_label_set_label(exams[i].timeLabel, "        ");
	        exams[i].blink++;
	     }else{
		    exams[i].blink = 0;  
	     }
      }else{  //se l'esame è finito da più di 2 minuti
         gtk_label_set_label(exams[i].timeLabel, "FINE");
         return FALSE;  //la funzione non verrà più chiamata
	  }
   }else{  //se l'esame non è finito
      sprintf(string,"%02d:%02d:%02d", h, m, s );
      gtk_label_set_label(exams[i].timeLabel, string);  //aggiorna il tempo rimanente nella casella di testo
   }

   return TRUE;  //la funzione verrà chiamata ancora

}


void key_pressed(GtkWindow *window){   //fa iniziare gli esami

   int e = 0;
  
   if(isStarted == FALSE){   //se gli esami non sono ancora iniziati
      time(&start);           //acquisisco l'ora di partenza
      isStarted = TRUE;       //gli esami sono iniziati
	
      for (e = 0; e < examsCount; e++){  //scorro il vettore degli esami
         g_timeout_add(100,set_exam, e);  //ogni 100ms chiamo la funzione che aggiorna le caselle di testo
      }
	
   }
   
}
