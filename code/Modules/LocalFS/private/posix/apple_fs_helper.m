#import <Foundation/Foundation.h>

bool get_ios_documents_dir( char *documents_dir, int maxlen )
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths firstObject];
    if (![documentsDirectory getCString:documents_dir maxLength:maxlen encoding:NSUTF8StringEncoding]) {
        return false;
    }
    return true;
}
