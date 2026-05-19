echo "[*] Compiling..."
rm ./main/OpenPetya.exe
x86_64-w64-mingw32-g++ -o ./main/OpenPetya.exe ./main/OpenPetya.cpp -lsetupapi -static
echo "[+] OK"
