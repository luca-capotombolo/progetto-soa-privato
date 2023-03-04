# Mi posiziono sulla directory dove è presente il codice da testare.
cd ~/Scrivania/progetto-soa/privato/progetto-soa-privato/file-system

# Rimuovo una eventuale compilazione che è stata precedentemente fatta.
make clean

# Compilo.
make all

# Installo il modulo.
sudo insmod file_system.ko

# Verifico se la registrazione del file system è andata a buon fine.
cat /proc/filesystems

# Creo il punto di montaggio.
mkdir -p /mnt/soafs

dmesg
