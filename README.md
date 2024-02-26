## Implementare

- segv_handler(int, siginfo_t, void*), handler care preia datele despre semnalul SIGSEGV inregistrat
si cauta adresa de page fault in segmentele de date din fisierul deschis in functia so_execute;
- page_mapper(so_seg_t, void*), functie care se ocupa de maparea efectiva a paginilor, respectand
cerintele de interceptare a page fault-urilor de pe pagina temei.
Mai intai aflu index-ul paginii pe care incerc s-o mapez pentru a accesa zona de memorie si verific
daca aceasta a mai fost mapata sau nu (am folosit vectorul void * data din structura so_seg_t).Daca este
deja mapata, folosesc functia check_mapping (descrisa mai jos).

In functie de pozitia paginii in segment, o mapez fie public (daca trebuie sa copiez date din segment),
fie anonim (daca pagina contine doar zerouri sau date care nu apartin segmentului).

Dupa mapare, incerc sa zeroizez zona dintre spatiul din memorie si spatiul din fisier al segmentului
pagina cu pagina. Daca pagina contine si date din fisier, dar si date din afara lui, retin in variabila
non_zero numarul de biti din fisier si zeroizez doar (page_size - non_zero) biti. Daca contine si date din
afara segmentului, zeroizez direct toata zona ceruta in enuntul temei. Odata mapata, o marchez in vectorul
data si folosesc functia mprotect pentru a mapa permisiunile segmentului.
- check_mapping (void*), functie care verifica daca maparea este valida. Am considerat ca pentru 
o eroare de mapare trebuie sa folosesc handler-ul default de page fault, doarece fara o mapare 
corecta adresa de page fault nu poate fi accesata. 
