package black.android.os;

import java.io.FileDescriptor;

import com.android.blackreflection.annotation.BClassName;
import com.android.blackreflection.annotation.BMethod;

@BClassName("android.os.MemoryFile")
public interface MemoryFile {
    @BMethod
    FileDescriptor getFileDescriptor();
}
