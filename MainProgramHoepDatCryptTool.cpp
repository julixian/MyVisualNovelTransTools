#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>

using namespace std;

uint8_t crypt(uint8_t a, uint8_t b) {
    return ~(a & b) & (a | b);
}

void decrypt2(vector<uint8_t>& enc_buf, vector<uint8_t>& dec_buf) {
    size_t k = 0;
    for (size_t i = 0; i < enc_buf.size(); i++) {
        enc_buf[i] = crypt(enc_buf[i], dec_buf[k++]);
        if (k == 16) {
            uint8_t key = enc_buf[i - 1];
            switch (key & 7) {
            case 0:
                dec_buf[0] += key;
                dec_buf[4] = dec_buf[2] + key + 11;
                dec_buf[3] += key + 2;
                dec_buf[8] = dec_buf[6] + 7;
                break;
            case 1:
                dec_buf[2] = dec_buf[9] + dec_buf[10];
                dec_buf[6] = dec_buf[7] + dec_buf[15];
                dec_buf[8] += dec_buf[1];
                dec_buf[15] = dec_buf[3] + dec_buf[5];
                break;
            case 2:
                dec_buf[1] += dec_buf[2];
                dec_buf[5] += dec_buf[6];
                dec_buf[7] += dec_buf[8];
                dec_buf[10] += dec_buf[11];
                break;
            case 3:
                dec_buf[9] = dec_buf[1] + dec_buf[2];
                dec_buf[11] = dec_buf[5] + dec_buf[6];
                dec_buf[12] = dec_buf[7] + dec_buf[8];
                dec_buf[13] = dec_buf[10] + dec_buf[11];
                break;
            case 4:
                dec_buf[0] = dec_buf[1] + 0x6f;
                dec_buf[3] = dec_buf[4] + 0x47;
                dec_buf[4] = dec_buf[5] + 0x11;
                dec_buf[14] = dec_buf[15] + 0x40;
                break;
            case 5:
                dec_buf[2] += dec_buf[10];
                dec_buf[4] = dec_buf[5] + dec_buf[12];
                dec_buf[6] = dec_buf[8] + dec_buf[14];
                dec_buf[8] = dec_buf[0] + dec_buf[11];
                break;
            case 6:
                dec_buf[9] = dec_buf[1] + dec_buf[11];
                dec_buf[11] = dec_buf[3] + dec_buf[13];
                dec_buf[13] = dec_buf[5] + dec_buf[15];
                dec_buf[15] = dec_buf[7] + dec_buf[9];
            default:
                dec_buf[1] = dec_buf[5] + dec_buf[9];
                dec_buf[2] = dec_buf[6] + dec_buf[10];
                dec_buf[3] = dec_buf[7] + dec_buf[11];
                dec_buf[4] = dec_buf[8] + dec_buf[12];
                break;
            }
            k = 0;
        }
    }
}

void encrypt2(vector<uint8_t>& enc_buf, vector<uint8_t>& dec_buf) {
    size_t k = 0;
    std::vector<uint8_t> buffer;
    for (size_t i = 0; i < enc_buf.size(); i++) {
        buffer.push_back(enc_buf[i]);
        enc_buf[i] = crypt(enc_buf[i], dec_buf[k++]);
        if (k == 16) {
            uint8_t key = buffer[buffer.size() - 2];
            switch (key & 7) {
            case 0:
                dec_buf[0] += key;
                dec_buf[4] = dec_buf[2] + key + 11;
                dec_buf[3] += key + 2;
                dec_buf[8] = dec_buf[6] + 7;
                break;
            case 1:
                dec_buf[2] = dec_buf[9] + dec_buf[10];
                dec_buf[6] = dec_buf[7] + dec_buf[15];
                dec_buf[8] += dec_buf[1];
                dec_buf[15] = dec_buf[3] + dec_buf[5];
                break;
            case 2:
                dec_buf[1] += dec_buf[2];
                dec_buf[5] += dec_buf[6];
                dec_buf[7] += dec_buf[8];
                dec_buf[10] += dec_buf[11];
                break;
            case 3:
                dec_buf[9] = dec_buf[1] + dec_buf[2];
                dec_buf[11] = dec_buf[5] + dec_buf[6];
                dec_buf[12] = dec_buf[7] + dec_buf[8];
                dec_buf[13] = dec_buf[10] + dec_buf[11];
                break;
            case 4:
                dec_buf[0] = dec_buf[1] + 0x6f;
                dec_buf[3] = dec_buf[4] + 0x47;
                dec_buf[4] = dec_buf[5] + 0x11;
                dec_buf[14] = dec_buf[15] + 0x40;
                break;
            case 5:
                dec_buf[2] += dec_buf[10];
                dec_buf[4] = dec_buf[5] + dec_buf[12];
                dec_buf[6] = dec_buf[8] + dec_buf[14];
                dec_buf[8] = dec_buf[0] + dec_buf[11];
                break;
            case 6:
                dec_buf[9] = dec_buf[1] + dec_buf[11];
                dec_buf[11] = dec_buf[3] + dec_buf[13];
                dec_buf[13] = dec_buf[5] + dec_buf[15];
                dec_buf[15] = dec_buf[7] + dec_buf[9];
            default:
                dec_buf[1] = dec_buf[5] + dec_buf[9];
                dec_buf[2] = dec_buf[6] + dec_buf[10];
                dec_buf[3] = dec_buf[7] + dec_buf[11];
                dec_buf[4] = dec_buf[8] + dec_buf[12];
                break;
            }
            k = 0;
        }
    }
}

bool decryptDatFile(const string& inputPath, const string& outputPath) {
    ifstream inFile(inputPath, ios::binary);
    if (!inFile) {
        cout << "Cant not open file: " << inputPath << endl;
        return false;
    }

    uint32_t magic;
    inFile.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != 0x01000000) {
        cout << "Not a valid MainProgramHoep dat file" << endl;
        return false;
    }

    ofstream outFile(outputPath, ios::binary);
    if (!outFile) {
        cout << "Can not create file: " << outputPath << endl;
        return false;
    }
    outFile.write((char*)&magic, 4);

    vector<uint8_t> decryptBuffer(16);
    inFile.read(reinterpret_cast<char*>(decryptBuffer.data()), 16);
    outFile.write((char*)decryptBuffer.data(), 16);

    inFile.seekg(0, ios::end);
    size_t fileSize = inFile.tellg();
    size_t dataSize = fileSize - 20;

    inFile.seekg(20);
    vector<uint8_t> data(dataSize);
    inFile.read(reinterpret_cast<char*>(data.data()), dataSize);
    inFile.close();

    decrypt2(data, decryptBuffer);

    outFile.write(reinterpret_cast<char*>(data.data()), data.size());
    outFile.close();

    cout << "Decrypting successfully！" << endl;
    return true;
}

bool encryptDatFile(const string& inputPath, const string& outputPath) {
    ifstream inFile(inputPath, ios::binary);
    if (!inFile) {
        cout << "Cant not open file: " << inputPath << endl;
        return false;
    }

    uint32_t magic;
    inFile.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != 0x01000000) {
        cout << "Not a valid MainProgramHoep dat file" << endl;
        return false;
    }

    ofstream outFile(outputPath, ios::binary);
    if (!outFile) {
        cout << "Can not create file: " << outputPath << endl;
        return false;
    }
    outFile.write((char*)&magic, 4);

    vector<uint8_t> decryptBuffer(16);
    inFile.read(reinterpret_cast<char*>(decryptBuffer.data()), 16);
    outFile.write((char*)decryptBuffer.data(), 16);

    inFile.seekg(0, ios::end);
    size_t fileSize = inFile.tellg();
    size_t dataSize = fileSize - 20; 

    inFile.seekg(20);
    vector<uint8_t> data(dataSize);
    inFile.read(reinterpret_cast<char*>(data.data()), dataSize);
    inFile.close();

    encrypt2(data, decryptBuffer);

    outFile.write(reinterpret_cast<char*>(data.data()), data.size());
    outFile.close();

    cout << "Encrypting successfully！" << endl;
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cout << "Made by julixian 2025.03.12" << endl;
        cout << "Usage: " << argv[0] << " <mode> <input_dat> <output_dat>" << "\n"
            << "mode: -d for decrypt, -e for encrypt" << endl;
        return 1;
    }

    string mode = argv[1];
    string inputPath = argv[2];
    string outputPath = argv[3];
    if (mode == "-d") {
        if (!decryptDatFile(inputPath, outputPath)) {
            cout << "Fail to decrpyt！" << endl;
            return 1;
        }
    }
    else if (mode == "-e") {
        if (!encryptDatFile(inputPath, outputPath)) {
            cout << "Fail to decrpyt！" << endl;
            return 1;
        }
    }
    else {
        cout << "Not a valid mode" << endl;
        return 1;
    }

    return 0;
}
