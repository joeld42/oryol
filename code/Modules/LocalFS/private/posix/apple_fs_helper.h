#pragma once

extern "C" {

// copies the ios/osx docuements directory into the buffer.
// if the directory is longer than maxlen, returns false
bool get_ios_documents_dir( char *documents_dir, int maxlen );

} // extern "C"
