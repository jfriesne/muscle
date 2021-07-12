MUSCLE Client Component for Delphi version 1.0.0

This is the first public release. Documentation is a bit thin on the ground, sorry!

LICENSING: Please see "license-delphi.txt" or the header of any source file to see the licensing terms for this code.

INSTALLING: open the included package, compile and then hit "install" on the package manager. Everything *should* work without a hitch under Delphi 5. Remember to include this directory in the Project or Global Library Path!

HISTORY: This is a basic implementation of the MUSCLE client side API. It works well enough to be a starting point for projects wishing to utilise MUSCLE in their Delphi applications.

I wrote this code based, more or less, on the Java (and bits of the C#) API, and so  it's not really like the C++ API. Delphi, being fairly single threaded, doesn't do anything clever with thread pooling and such. We just use a TClientSocket and put that into non-blocking mode and hope for the best.

The component and classes, except for the TPortableMessage, have NOT been thoroughly tested. The TPortableMessage code is very old, and has been extensively tested in real world situations - but still may have bugs, errors and incompatibilities with the MUSCLE::Message. The basic Types all work though (string, intXX and bool) and any others will get an overhaul in the next release. I cannot verify that any types past these will work for this release!

The IPortableMessage interface should be used as much as possible. I realise the use of interfaces may not be to everyones taste, but they make life a lot easier, and so they are here to stay.

There are some oddites in the Message.pas unit still. There is a class callet TmuscleMessage, and an interface of a similar name. This was the original port, before I sugar coated it for use in another application (that already used a similar interface, so I adapted the TmuscleMessage to TPortableMessage to use the same legacy interface.) The TPortableMessage was called the TBMessage originally, and if you see any code refering to a TBMessage or equiv interface, the class in question is the TPortableMessage or equiv interface.

THIS CODE HAS ONLY BEEN TESTED UNDER DELPHI 5 ENTERPRISE AND DELPHI 5 PROFESSIONAL. IT MAY WORK WITH LATER VERSIONS WITH OUT CHANGES, BUT I ASSUME SOME TWEAKS MAY BE REQUIRED. I plan to update the code to Delphi 7 at a later date. Delphi 8/2005 for dotNET will be unsupported. Use the CSharp classes instead!