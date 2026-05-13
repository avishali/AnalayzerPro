PACE’s digital signing technology leverages the operating system’s digital signature support and extends it with our own.

wraptool first calls Windows’ SignTool or Apple’s codesign to perform the binary digital signing operations required by the platform, with the appropriate platform certificate. Then wraptool signs that signature using a PACE code-signing certificate. There is no need to digitally sign again separately after signing with wraptool.

The operating system will recognize the platform’s signature, and will not even know about the PACE signature. Fusion-protected host ecosystems (Pro Tools) can make simple, cross-platform Fusion API calls to verify not only that the binary was signed correctly for the platform, but that it was signed by a PACE publisher (you). They can also determine which publisher performed the signing, which product was signed, and other signing details.

Cloud Signing Service

In some build environments, such as building in the cloud, iLok USBs cannot be used for signing. To address this problem, PACE provides an internet service that supports code signing in place of an iLok USB. This signing service is referred to as Cloud Signing or the Cloud Signing Service.

Please contact sales@paceap.com if you are interested in this service.

In order to digitally sign your products, a second-generation or newer iLok USB must be certified by PACE with your customer ID. This will create a special PACE code-signing certificate inside the iLok USB itself that contains information about your company. If your account is associated with multiple publishers, then the certificate will contain the credentials to sign code for any of those publishers.

Note

The iLok USB is only compatible with PACE code-signing certificates, as it is designed to work specifically with our proprietary security technology. It does not support storing digital certificates from any certificate authorities other than PACE Anti-Piracy.

If you authorized your Eden Tools license to an iLok USB during the activation process, we recommend that you certify that same iLok USB for signing. That way, you only need a single iLok USB to both authorize the AAX Code Signing Tools, and digitally sign your binaries. However, the Eden Tools authorization and the signing certificate are not required to be on the same iLok USB. If you wish to have the signing certificate on a separate iLok USB from your Eden Tools authorization, you may do so.

Important!

The word ‘certificate’ is used to refer both to the platform-specific signing certificate, and to the internal certificate in a second-generation or later iLok USB that enables it to perform signing. Some error messages from wraptool can refer to either kind of certificate, so it’s important to be aware of this dual meaning.

Get the appropriate certificate for your platform.

Get the PACE code-signing certificate.

Certificate Expiration

The PACE code-signing certificate has an expiration date. When it expires, renew it by repeating these iLok License Manager steps.

Open the “iLok License Manager” application that was installed with the AAX Code Signing Tools:

On Mac: find it in /Applications.
On Windows: use the shortcut in the Start Menu or locate it in Program Files\iLok License Manager\iLok License Manager.exe.
Sign in to your iLok account.

Insert the iLok USB you wish to certify.
Certify the iLok USB:
Option 1: Right-click on the iLok USB and select “Synchronize”.
Option 2: Click “Show Details” at the bottom right, then select “Synchronize” in the details pane.
iLok2 Certificate iconOnce synchronized, the iLok USB’s icon will include a round “seal” behind the main image, distinguishing it from uncertified devices.




Sign Your Plug-In
To sign your product, you provide wraptool with your account ID, wrap config GUID, and other options related to signing. This allows wraptool to communicate with our services and download whatever materials are needed for the signing operation, including the latest version of our signing-related binaries, and your latest wrap config revision.

Review the System Requirements and follow the Installation Instructions.
Get signing certificate(s), both for your platform(s) and for PACE signing.
Create your Product on PACE Central.

Navigate to CATALOG > Products and click the New button.
Enter the Product Name. All other available fields are optional.
When you are done, click Save as Active.
Product Editor

Create your Wrap Config on PACE Central.

The Wrap Configuration stores the information that wraptool needs in order to sign your product.

Navigate to CONFIGURE > Wrap Configuration and click the New button.
Enter the Name. All other fields are optional.

Default selections are used for the Wrap Configuration’s required attributes. Since you are not wrapping your Product, these settings do not apply. Ignore them.

On the Authorization Key tab, click the Select Product button and select the Product you already created.

When you are done, click Create.
Authorization Key Tab

Use wraptool sign to sign your product.

On PACE Central, navigate to CONFIGURE > Wrap Configuration and select the wrap config for your product.
Click the Command Line button under the list of wrap configurations to display the command line for digital signing with the GUID of the selected wrap config preloaded.
Copy the command line to your terminal and make appropriate edits (your ID, your signing credentials, your unsigned binary, your signed binary).

Important!

To support Avid’s requirements for AAX, make sure you are signing the plugin itself. Installers do not need to be signed with wraptool.
Keep in mind that there are signing syntax differences between platforms, so a wraptool command line that works on macOS will not necessarily work for Windows without edits.
When providing an account and password to wraptool, you only have to provide the password once. The password will be stored in your OS’s secure storage, so you won’t need to provide it again on the command line unless you change your account password.
See wraptool sign for more details. For information about macOS code signing in general, please see Notarizing and Apple’s documentation, including tech note TN2206.

Use wraptool verify to make sure everything worked.


Install the Tools
Standard Installation🔗
Obtain the PACE AAX Code Signing Tools🔗
If you have not already done so, access PACE Central.
Download EdenSDKLiteInstallerMac.zip or EdenSDKLiteInstallerWin64.zip from the Developer > Eden Tools Downloads (SDK) section of PACE Central.

CONFIDENTIAL!

The files available for download on PACE Central are confidential as per your PACE Anti-Piracy license or non-disclosure agreement with PACE Anti-Piracy. You may only provide them to employees or contractors who are covered under your company’s confidentiality agreements.

Downloads

Install the PACE AAX Code Signing Tools🔗
Review the System Requirements.
Uninstall previous versions.
Run the installer and follow the prompts to install the tools.
Activate PACE licenses🔗
As a new PACE customer, your iLok account will receive one or more licenses that you may need to use. These licenses have to be activated to a location like an iLok USB or iLok Cloud. You can activate these licenses by using iLok License Manager.

License	Purpose	Who needs it	Where to activate it
PACE Central Access	Launch PACE Central	Anyone who needs access to PACE Central to administer user access settings, download the PACE AAX Code Signing Tools, create products, and configure Wrap Configs	Personal iLok USB
Eden Tools	Run the AAX code signing tools	Developers using the tools, and CI/automated build machines	An iLok USB or iLok Cloud session attached to the machine running the Fusion tools
What if I need more Eden Tools licenses?
Advanced installation scenarios🔗
Virtual machines🔗
To authorize the AAX code signing tools on both a host OS and virtual machines running on that host:

Install the tools on your host and virtual machines. All instances of the tools must be updated in order for this to work.

Use iLok License Manager on the host machine to synchronize the iLok USB that holds your Eden Tools license and/or PACE code-signing certificate. This will update the iLok USB to allow it to be shared with your VMs.

Unplug and re-plug your iLok. This only needs to be done once, but it is critical to make this feature work.

After following the above instructions, your iLok USB should work on your host and VMs. However please be aware of the following restrictions and potential issues:

We have no mechanism to arbitrate iLok hardware between the host and your VMs. In fact, each operating system will have its own LicenseD service running.

Depending on the virtualization software you are running, you may need to assign USB access to an iLok USB to a given VM. If you cannot do this automatically, you may still require multiple iLok USBs for automatic builds. Since PACE has no visibility into host/VM USB arbitration, there is nothing we can do about USB assignment restrictions imposed by your VM software.

You should not perform simultaneous builds on multiple operating systems using the same iLok USB for authorization. This is probably a non-issue if your VM software limits USB access to a single OS at a time.

When you update your signing certificate in the future to refresh the expiration date, you also may need to unplug and re-plug your iLok USB before you can successfully share it with your VMs.

This feature of sharing iLok USBs between operating systems is only available to PACE customers using iLok USBs that have been certified for digital signing. For security reasons, we cannot make this feature available to the general public.

Silent, unattended installs🔗
All PACE installers support silent and unattended execution, but it is the customer’s responsibility to configure and create the silent installation process according to their specific requirements.

The iLok driver merge module contains logic to detect the case where a Windows 7 system does not have the updates required to support the iLok driver’s SHA256 signature. If you use the iLok driver merge module in your installer (see instructions above), your installer will automatically contain that behavior, including falling back to a SHA1 certificate.

To create a silent install, the first step is to build a response file (with a .iss extension) to specify the options to use when running the installer in unattended mode. To automatically generate a response file, run the installer with the -r command line switch on your setup.exe command. While running the installer, specify exactly the options desired for the installer’s unattended operation. When you finish the installation, the setup.iss file will be created in the Windows system folder. (You can also specify where to store the .iss file with the -f1 option, for example, setup.exe -r -f1"C:\\Temp\\FullInstall.iss".)

To run a silent install, have all the necessary setup and support files (including the setup.iss file) in a single directory. Run the setup executable file (the .exe) using the -s switch on the command line. (You can specify an alternate .iss file name/location using the -f1 option, for example using, setup.exe -s -f1".\\FullInstall.iss".)

Tip

The silent install files are named setup.iss by default. They are not interchangeable, so be sure to keep each setup.iss file with its installer.

Why is my silent installation of the AAX Code Signing Tools for a CI workflow failing/stalling?

We expect to fix this known issue in Fusion 6.0.

The installer is probably trying to display a dialog message or other type of alert. Fortunately, this behavior usually occurs at the end of the installation process, when all necessary items have already been installed. If you kill the process at this point, the installation still completes.

You can work around this for containers by calling the install command from a Powershell script and waiting for the installation hang.

Example script contents

Start-Process -FilePath "c:\temp\Eden 5 SDK Lite Win64.exe" -ArgumentList '/S', '/v"/qn /L*V c:\msi_log.txt"', '/f1"c:\temp\Eden 5 SDK Lite Win64.iss"', '/debuglog"c:\debug.log"', '/d'
Start-Sleep -Seconds 60
You can use similar techniques on other platform-as-a-service solutions, to script a wait and then kill the process.

Installed components🔗
PACE AAX Code Signing Tools

The tools are installed at:


macOS
Windows

/Applications/PACEAntiPiracy/Eden/Fusion/Versions/5

PACE License Support

The PACE AAX Code Signing Tools installer will also install the end-user License Support components, including iLok License Manager:


macOS
Windows

/Applications/iLok License Manager.app

## See also

- [release_macos.md](release_macos.md) — build → PACE AAX → Apple sign → installer → notarization order and `release_macos.sh`.

