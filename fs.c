/*
 * RSFS - Really Simple File System
 *
 * Copyright © 2010,2012,2019 Gustavo Maciel Dias Vieira
 * Copyright © 2010 Rodrigo Rocco Barbieri
 *
 * This file is part of RSFS.
 *
 * RSFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "disk.h"
#include "fs.h"

#define CLUSTERSIZE 4096
#define FATCLUSTERS 65536
#define DIRENTRIES 128
#define MAX_OPEN_FILES 32
unsigned short fat[FATCLUSTERS];

typedef struct {
  char used;
  char name[25];
  unsigned short first_block;
  int size;
} dir_entry;

dir_entry dir[DIRENTRIES];

char inicializado; // Variavel importante que indica se o sistema de arquivos
                   // está inicializado. todas as funções exceto init e format
                   // não devem executar e retornar erro se essa variável for 0.

                                        // Array para armazenar identificadores de arquivos abertos
int open_files[MAX_OPEN_FILES] = {-1}; // Inicialmente, todos os slots estão livres

                                    // Função para encontrar um identificador de arquivo livre
int get_free_file_descriptor() {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i] == -1) {
            return i;
        }
    }
    return -1; // Se não houver slots livres
}

// fUNÇÃO PARA ZERAR DIRETORIO;
int inicializar_dir() {
  for (int i = 0; i < DIRENTRIES; i++) {
    dir[i].used = 0;
  }

  // carrega o dir no disco
  char *buffer;
  buffer = (char *)dir;

  return bl_write(32, buffer);
}

// Função para carregar um diretório do disco para a memória;
int carregar_dir() {
  char *buffer;
  buffer = (char *)dir;

  return bl_read(32, buffer);
}

// funcao para salvar o diretorio na memoria
int salvar_dir() {
  char *buffer;
  buffer = (char *)dir;

  return bl_write(32, buffer);
}

// função para inicializar a FAT
int inicializar_FAT() {
  for (int i = 0; i < 32; i++)
    fat[i] = 3;

  fat[32] = 4;

  for (int i = 33; i < FATCLUSTERS; i++)
    fat[i] = 1;

  // carrega a FAT do disco
  char *buffer;
  buffer = (char *)fat;

  for (int i = 0; i < 32; i++) {
    if (!bl_write(i, &buffer[i * SECTORSIZE]))
      return 0;
  }

  return 1;
}

// funcao para salvar a FAT no disco
int salvar_FAT() {
  char *buffer;
  buffer = (char *)fat;

  for (int i = 0; i < 32; i++) {
    if (!bl_write(i, &buffer[i * SECTORSIZE]))
      return 0;
  }

  return 1;
}

// Funcao que converte um int para uma string
// retirada de
// https://www.geeksforgeeks.org/how-to-convert-an-integer-to-a-string-in-c/

void intToStr(int N, char *str) {
  int i = 0;


  //modificação para funcionar com size 0
  if(N == 0){
    str[0] = 48;
    str[1] = '\0';
    return;
  }
  
  // Save the copy of the number for sign
  int sign = N;

  // If the number is negative, make it positive
  if (N < 0)
    N = -N;

  // Extract digits from the number and add them to the
  // string
  while (N > 0) {

    // Convert integer digit to character and store
    // it in the str
    str[i++] = N % 10 + '0';
    N /= 10;
  }

  // If the number was negative, add a minus sign to the
  // string
  if (sign < 0) {
    str[i++] = '-';
  }

  // Null-terminate the string
  str[i] = '\0';

  // Reverse the string to get the correct order
  for (int j = 0, k = i - 1; j < k; j++, k--) {
    char temp = str[j];
    str[j] = str[k];
    str[k] = temp;
  }
}

int fs_init() {
  char *buffer;
  buffer = (char *)fat;
  // copiar os primeiros 32 setores do disco para o fat
  for (int i = 0; i < 32; i++)
    if (!bl_read(i, &buffer[i * SECTORSIZE]))
      return 0; // erro

  // vereficar se o fat esta formatado.
  for (int i = 0; i < 32; i++)
    if (fat[i] != 3) {
      printf("Sistema de arquivos não formatado.\n");
      inicializado = 0;
      return 1;
    }
  if (fat[32] != 4) {
    printf("Sistema de arquivos não formatado.\n");
    inicializado = 0;
    return 1;
  }

  if (!carregar_dir()) {
    return 0;
  }
  inicializado = 1;
  return 1;
}

int fs_format() {
  if (!inicializar_FAT() || !inicializar_dir())
    return 0;
  inicializado = 1; // toda vez que formatar o disco, o sistema de arquivos esta
                    // inicializado.
  return 1;
}

int fs_free() {
  if (inicializado == 0) {
    printf("Sistema de arquivos nao formatado.\n");
    return 0;
  }

  int t = bl_size();
  int free = 0;
  for (int i = 33; i < t; i++) {
    if (fat[i] == 1)
      free++; // calcula quantos setores estão livres
  }

  return free * SECTORSIZE; // retorna o tamanho livre em bytes
}

int fs_list(char *buffer, int size) {

    if (inicializado == 0) {
        printf("Sistema de arquivos nao formatado.\n");
        return 0;
    }

    int c = 0; // Cabeça de escrita

    for (int i = 0; i < DIRENTRIES; i++) {

        if (dir[i].used == 1) {

            for (int j = 0; j < 25; j++)
                
                if (dir[i].name[j] != '\0') {
                    
                    if (c < size - 2) // Verifica se há espaço suficiente
                        buffer[c++] = dir[i].name[j];
                    else {
                        buffer[c] = '\0'; // Finaliza a string
                        return 1; // Buffer cheio
                    }
                }

            if (c < size - 4) {
                buffer[c++] = 9; // tab
                buffer[c++] = 9; // tab
                intToStr(dir[i].size, &buffer[c]);

                // Mover cabeça de escrita até o local correto
                while (buffer[c] != '\0') {
                    if (c >= size - 1) {
                        buffer[c] = '\0'; // Finaliza a string
                        return 1; // Buffer cheio
                    }
                    c++;
                }

                buffer[c++] = '\n'; // quebra de linha
            } else {
                buffer[c] = '\0'; // Finaliza a string
                return 1; // Buffer cheio
            }
        }
    }

    buffer[c] = '\0'; // Finaliza a string
    return 1;

}

int fs_create(char *file_name) {
  if (inicializado == 0) {
    printf("Sistema de arquivos nao formatado.\n");
    return 0;
  }

  // antes: ver se o arquivo existe
  for (int i = 0; i < DIRENTRIES; i++)
    if (dir[i].used == 1 && strcmp(dir[i].name, file_name) == 0) {
      printf("Arquivo ja existe.\n");
      return 0;
    }

  // primeira parte: encontrar no vetor de diretorios um espaço
  int i;

  for (i = 0; i < DIRENTRIES; i++)
    if (dir[i].used == 0)
      break;

  if (i == DIRENTRIES)
    return 0; // nao tem espaço

  // segunda parte: encontrar um setor livre para o arquivo

  int j;
  int t = bl_size();
  for (j = 33; j < t; j++)
    if (fat[j] == 1)
      break;

  if (j == t)
    return 0; // nao tem setor livre

  // fazer o arquivo
  dir[i].used = 1;
  strcpy(dir[i].name, file_name);
  dir[i].first_block = j;
  dir[i].size = 0;
  // atualizar a fat
  fat[j] = 2;

  // carregar as novas informações no disco
  salvar_dir();
  salvar_FAT();

  return 1;
}

int fs_remove(char *file_name) {

    // Verificar se o sistema de arquivos está inicializado
    if (inicializado == 0) {
        printf("Sistema de arquivos não formatado.\n");
        return 0;
    }

    // Encontrar o arquivo no diretório
    int file_index = -1;
    for (int i = 0; i < DIRENTRIES; i++)
        if (dir[i].used && strcmp(dir[i].name, file_name) == 0) {
            file_index = i;
            break;
        }

    if (file_index == -1) {
        printf("Arquivo %s não encontrado.\n", file_name);
        return 0;
    }

    // Liberar os blocos de dados ocupados pelo arquivo
    unsigned short block = dir[file_index].first_block;

    while (block != 0) {
        unsigned short next_block = fat[block];
        fat[block] = 1; // Marca o bloco como livre
        block = next_block;
    }

    // Remover o arquivo do diretório
    dir[file_index].used = 0;

    // Salvar as alterações no diretório e na FAT
    if (!salvar_dir() || !salvar_FAT()) {
        printf("Falha ao salvar o diretório ou a FAT.\n");
        return 0;
    }

    return 1; // Sucesso:wq!
}

int fs_open(char *file_name, int mode) {
  
    int file_index = -1;
    
    for (int i = 0; i < DIRENTRIES; i++) {
        if (dir[i].used && strcmp(dir[i].name, file_name) == 0) {
            file_index = i;
            break;
        }
    }

    if (file_index == -1) {
        printf("Arquivo não encontrado.\n");
        return -1;
    }

    // Verifica se o arquivo já está aberto ou se há espaço para mais arquivos
    int fd = get_free_file_descriptor();

    if (fd == -1) {
        printf("Número máximo de arquivos abertos alcançado.\n");
        return -1;
    }

    open_files[fd] = file_index;

    return fd;    
}

int fs_close(int file) {

    if (!inicializado) {
        printf("Sistema de arquivos não formatado.\n");
        return 0;
    }

    if (file < 0 || file >= MAX_OPEN_FILES || open_files[file] == -1) {
        printf("Identificador de arquivo inválido.\n");
        return 0;
    }

    open_files[file] = -1;
    
    return 1;
}

int fs_write(char *buffer, int size, int file) {

    if (!inicializado) {
        printf("Sistema de arquivos não formatado.\n");
        return -1;
    }

    if (file < 0 || file >= MAX_OPEN_FILES || open_files[file] == -1) {
        printf("Identificador de arquivo inválido.\n");
        return -1;
    }

    int file_index = open_files[file];
    dir_entry *entry = &dir[file_index];
    int blocks_needed = (size + CLUSTERSIZE - 1) / CLUSTERSIZE;

    unsigned short current_block = entry->first_block;
    int i;

    for (i = 0; i < blocks_needed; i++)

        if (fat[current_block] == 2 || fat[current_block] == 1) {

            unsigned short new_block = -1;

            for (unsigned short j = 33; j < FATCLUSTERS; j++)

                if (fat[j] == 1) {
                    new_block = j;
                    break;
                }

            if (new_block == (unsigned short)-1)
                return -1; // Sem espaço suficiente

            fat[current_block] = new_block;
            fat[new_block] = (i == blocks_needed - 1) ? 2 : 1; // Marca o último bloco como 2
            current_block = new_block;
        }

    entry->size = size;
    salvar_dir();
    salvar_FAT();

    // Escreve os dados nos blocos
    int bytes_written = 0;
    current_block = entry->first_block;
    
    while (size > 0) {

        char sector_buffer[CLUSTERSIZE];
        int to_write = (size > CLUSTERSIZE) ? CLUSTERSIZE : size;
        
        memcpy(sector_buffer, buffer + bytes_written, to_write);
        
        if (!bl_write(current_block, sector_buffer))
            return -1;
        
        size -= to_write;
        bytes_written += to_write;
        current_block = fat[current_block];
    }

    return bytes_written; 
}

int fs_read(char *buffer, int size, int file) {

    if (!inicializado) {
        printf("Sistema de arquivos não formatado.\n");
        return -1;
    }

    if (file < 0 || file >= MAX_OPEN_FILES || open_files[file] == -1) {
        printf("Identificador de arquivo inválido.\n");
        return -1;
    }

    int file_index = open_files[file];

    dir_entry *entry = &dir[file_index];
    
    int bytes_read = 0;
    unsigned short current_block = entry->first_block;

    while (size > 0 && current_block != 2) {

        char sector_buffer[CLUSTERSIZE];
        
        if (!bl_read(current_block, sector_buffer))
            return -1;
        
        int to_read = (size > CLUSTERSIZE) ? CLUSTERSIZE : size;

        memcpy(buffer + bytes_read, sector_buffer, to_read);
        size -= to_read;
        bytes_read += to_read;
        current_block = fat[current_block];
    }

    return bytes_read;
}
