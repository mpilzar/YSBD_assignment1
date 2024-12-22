#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bf.h"
#include "hp_file.h"
#include "record.h"

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    BF_PrintError(code);    \
    return HP_ERROR;        \
  }                         \
}

#define HP_ERROR -1

int HP_CreateFile(char *fileName){

  int fd1;
  BF_Block* block;
  char* data;

  CALL_BF(BF_CreateFile(fileName));             // Δημιουργία αρχείου
  CALL_BF(BF_OpenFile(fileName, &fd1));         // Άνοιγμα του αρχείου
  BF_Block_Init(&block);                        // Δέσμευση μνήμης για το block
  CALL_BF(BF_AllocateBlock(fd1, block));        // Δέσμευση του block

  HP_info hp_info;
  
  hp_info.last_block_id = 0;                    // Το id του τελευταίου block είναι 0 γιατί υπάρχει μόνο ένα block στη μνήμη
  hp_info.number_of_blocks = 1;                 // Μόνο ένα block στην μνήμη

  data = BF_Block_GetData(block);               // Λήψη δεδομένων από το block
  memcpy(data, &hp_info, sizeof(HP_info));      // Αντιγραφή των μεταδεδομένων στη data

  BF_Block_SetDirty(block);                     // Άλλαξαν τα περιεχόμενα του block
  CALL_BF(BF_UnpinBlock(block));                // Ξεκαρφίτσωμα του block
  BF_Block_Destroy(&block);                     // Καταστροφή του block
  CALL_BF(BF_CloseFile(fd1));                   // Κλείσιμο του αρχείου

  return 0;
}

HP_info* HP_OpenFile(char *fileName, int *file_desc){

  static HP_info hp_info;
  BF_Block* block; 
  char* data;   

  BF_OpenFile(fileName, file_desc);           // Άνοιγμα του αρχείου
  BF_Block_Init(&block);                      // Αρχικοποίηση του block
  BF_GetBlock(*file_desc, 0, block);          // Ανάγνωση του πρώτου block
  
  data = BF_Block_GetData(block);             // Λήψη δεδομένων από το block
  memcpy(&hp_info, data, sizeof(HP_info));    // Αντιγραφή των δεδομένων στη hp_info

  BF_UnpinBlock(block);                       // Ξεκαρφίτσωμα του block
  BF_Block_Destroy(&block);                   // Καταστροφή του block

  return &hp_info;
}


int HP_CloseFile(int file_desc, HP_info* hp_info){

  CALL_BF(BF_CloseFile(file_desc));
  return 0;
}

int HP_InsertEntry(int file_desc, HP_info* hp_info, Record record) {
  
  BF_Block *block;
  BF_Block_Init(&block);

  int block_id = hp_info->last_block_id;            // Παίρνουμε το id του τελευταίου block 

  if (block_id == 0) {                              // Αν το block είναι το πρώτο block στη μνήμη

    CALL_BF(BF_AllocateBlock(file_desc, block));    // Δημιουργούμε νέο block για τις εγγραφές
        
    char* data = BF_Block_GetData(block);           // Λήψη δεδομένων από το block
    memcpy(data, &record, sizeof(Record));          // Αντιγράφουμε την εγγραφή στο block

    hp_info->number_of_blocks = 1;                  // Υπάρχει ένα block στη μνήμη
    hp_info->first_block_rec = 1;                   // που είναι το πρώτο με εγγραφή
    hp_info->last_block_id = 1;

    HP_block_info block_info;
    block_info.record_count = 1;                    // Υπάρχει μια εγγραφή
    block_info.next_block_id = -1;                  // Δεν υπάρχει άλλο block στην μνήμη
    block_info.current_block_capacity = BF_BLOCK_SIZE - sizeof(HP_block_info) - sizeof(Record); // Υπολογισμός χωρητικότητας
        
    memcpy(data + BF_BLOCK_SIZE - sizeof(HP_block_info), &block_info, sizeof(HP_block_info));   // Αντιγραφή των δεδομένων του block στο τέλος του

    BF_Block_SetDirty(block);                       // Άλλαξαν τα περιεχόμενα του block
    BF_UnpinBlock(block);                           // Ξεκαρφίτσωμα του block
    }
    else {                                          // Αν υπάρχουν και άλλα block 

      CALL_BF(BF_GetBlock(file_desc, block_id, block));               // Φέρνουμε το τελευταίο block

      char* data = BF_Block_GetData(block);
      HP_block_info* block_info = (HP_block_info*)(data + BF_BLOCK_SIZE - sizeof(HP_block_info)); //Φέρνουμε τα δεδομένα του block

      if (block_info->current_block_capacity < sizeof(Record)) {      // Αν το τωρινό block είναι γεμάτο δημιουργούμε νέο

        BF_UnpinBlock(block);                                         // Δεν το χρειαζόμαστε πλέον στην μνήμη

        CALL_BF(BF_AllocateBlock(file_desc, block));                  // Δέσμευση νέου block

        data = BF_Block_GetData(block);                               // Φέρνουμε τα δεδομένα του νέoυ block 
        memcpy(data, &record, sizeof(Record));                        // Αντιγράφουμε την εγγραφή στο νέο block

        hp_info->number_of_blocks++;                                  // Αύξηση του μετρητή
        hp_info->last_block_id = hp_info->number_of_blocks;           // Το id του τελευταίου block ισούται με τον αριθμό block που υπάρχουν στη μνήμη

        HP_block_info new_block_info;
        new_block_info.record_count = 1;
        new_block_info.next_block_id = -1;                            // Δεν υπάρχει άλλο block στην μνήμη
        new_block_info.current_block_capacity = BF_BLOCK_SIZE - sizeof(HP_block_info) - sizeof(Record); // Υπολογισμός χωρητικότητας

        memcpy(data + BF_BLOCK_SIZE - sizeof(HP_block_info), &new_block_info, sizeof(HP_block_info));   // Αντιγραφή των νέων δεδομένων στο νέο block

        BF_Block* prev_block;
        BF_Block_Init(&prev_block);
        if (BF_GetBlock(file_desc, block_id, prev_block) == BF_OK) {    // Ενημέρωση του προηγούμενου block
          char* prev_data = BF_Block_GetData(prev_block);
          HP_block_info* prev_block_info = (HP_block_info*)(prev_data + BF_BLOCK_SIZE - sizeof(HP_block_info));
          prev_block_info->next_block_id = hp_info->last_block_id;
          BF_Block_SetDirty(prev_block);                                // Αλλάξαμε το περιεχόμενο του
          BF_UnpinBlock(prev_block);                                    // Δεν το χρειαζόμαστε πλέον στην μνήμη
        }

        BF_Block_Destroy(&prev_block);                                  // Καταστρέφουμε το προηγόυμενο block
        } 
        else {                                                          // Το τωρινό block ΔΕΝ είναι γεμάτο άρα εισάγουμε τις εγγραφές
          memcpy(data + (block_info->record_count * sizeof(Record)), &record, sizeof(Record));
          block_info->record_count++;
          block_info->current_block_capacity -= sizeof(Record);
        }
        
        BF_Block_SetDirty(block);       // Άλλαξαν τα περιεχόμενα του block
        BF_UnpinBlock(block);           // Ξεκαρφίτσωμα του block
    }

    BF_Block_Destroy(&block);           // Καταστροφή του block
    
    return hp_info->last_block_id;      // Επιστροφή του id του τελευταίου block
}

int HP_GetAllEntries(int file_desc, HP_info* hp_info, int value){    
  
  int blocks_read = 0;                      // Πόσα block έχουν διαβαστεί
  int block_id = hp_info->first_block_rec;  // Ξεκινάμε από το πρώτο block
  BF_Block* block;
  BF_Block_Init(&block);

  while (block_id != -1) {                  // Επανάληψη μέχρι το τελευταίο block
    CALL_BF(BF_GetBlock(file_desc, block_id, block));
    blocks_read++;                          // Αύξηση του μετρητή 

    char* data = BF_Block_GetData(block);
    HP_block_info* block_info = (HP_block_info*)(data + BF_BLOCK_SIZE - sizeof(HP_block_info));  // Αποθήκευση περιεχομένων του block

    for (int i = 0; i < block_info->record_count; i++) {            // Διάσχιση όλων των εγγραφών του κάθε block
      Record* record = (Record*)(data + i * sizeof(Record));
      if (record->id == value)
        printf("Record ID: %d, Name: %s, Surname: %s, City: %s\n", record->id, record->name, record->surname, record->city);
    }

    block_id = block_info->next_block_id;
    CALL_BF(BF_UnpinBlock(block));  // Ξεκαρφίτσωμα του block
  }

  BF_Block_Destroy(&block);         // Καταστροφή του block

  return blocks_read;               // Επιστροφή του αριθμού των block που διαβάστηκαν
}
