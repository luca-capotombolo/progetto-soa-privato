#include <linux/fs.h>
#include <linux/string.h>

#include "../headers/main_header.h"

/*
 * Questa funzione itera sulle entry di una directory (i.e., elenca il contenuto della directory).
 * Viene invocata dalla system call 'readdir'. La funzione verrà chiamata consecutivamente fino
 * alla lettura di tutte le entry disponibili. Restituisce zero se non ci sono più voci da leggere.
 * 
 */
static int soafs_iterate(struct file *file, struct dir_context* ctx) {

    printk("%s: E' stata invocata la iterate.", MOD_NAME);
	
    /* Non possiamo restituire più di '.', '..' e l'unico file. */
	if(ctx->pos >= (2 + 1))
    {
        return 0;
    }

	if (ctx->pos == 0)
    {
        
        /*
         * Gli argomenti della funzione dir_emit sono i seguenti:
         * 1. Il contesto di iterazione della directory.
         * 2. Il nome della entry.
         * 3. La lunghezza del nome della entry.
         * 4. Il numero dell'inode associato alla entry
         * 5. La tipologia della entry.
         */
		if(!dir_emit(ctx,".", SOAFS_MAX_NAME_LEN, SOAFS_ROOT_INODE_NUMBER, DT_UNKNOWN))
        {
			return 0;
		}
		else
        {
			ctx->pos++;
		}
	
	}

	if (ctx->pos == 1)
    {
		//here the inode number does not care
		if(!dir_emit(ctx,"..", SOAFS_MAX_NAME_LEN, 1, DT_UNKNOWN))
        {
			return 0;
		}
		else
        {
			ctx->pos++;
		}
	
	}

	if (ctx->pos == 2)
    {
		if(!dir_emit(ctx, SOAFS_UNIQUE_FILE_NAME, SOAFS_MAX_NAME_LEN, SOAFS_FILE_INODE_NUMBER, DT_UNKNOWN)){
			return 0;
		}
		else{
			ctx->pos++;
		}	
	}

	return 0;

}

const struct file_operations soafs_dir_operations = {
    .owner = THIS_MODULE,
    .iterate = soafs_iterate,
};
