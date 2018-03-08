#!/usr/bin/perl
#
use strict;
use diagnostics;
use warnings;
use File::Path;
use Net::FTP;
use File::Copy qw(copy);
use List::Util 'first';  

use Cwd;

#   ARGS:
#
# require pkgs:
#   yum -y install anaconda createrepo mkisofs rsync yum-utils genisoimage anaconda squashfs-tools
# mysql:
#   yum install -y ./Product/config/cfg/mysql57-community-release-el7-11.noarch.rpm
# epel:
#   yum install -y epel-release.noarch
# need to create local repo to increase rebuild spead:
#   init_centos7repo($vmiiso_pkgdir,$vmiiso_rpmcfg,$vmiiso_localrpmcfg);
#   

my $vmiiso_pkgdir   ="/home/jenkinsbuild/ci-jenkins/workspace/TMVMI/centos7repo/";
#   for local debug, if not build in ci machine, set to /builddir/600src/centos7repo/
$vmiiso_pkgdir   ="/builddir/600src/centos7repo/" if (! -d '/home/jenkinsbuild/ci-jenkins/workspace/TMVMI/');

my $curdir              = "."; #getcwd();
my $vmiiso_tmpdir       ="$curdir/tmp/";
my $vmiiso_vmipkgdir    ="$curdir/vmipkgs";

my $vmiiso_cfgdir       ="$curdir/Product/config/cfg";
my $vmiiso_rpmcfg       = "$vmiiso_cfgdir/rpms.txt";
my $vmiiso_localrpmcfg  = "$vmiiso_cfgdir/rpms_local.txt";

my $bin_yumdownloader   ="/usr/bin/yumdownloader";
my $bin_rsync           ="/usr/bin/rsync";
my $bin_createrepo      ="/usr/bin/createrepo";
my $bin_genisoimage     ="/usr/bin/genisoimage";
my $bin_checkisomd5     ="/usr/bin/checkisomd5";

my %ftpinfo             = (
                    host => '10.204.16.2',
                    usr  => 'su-tmvmi6',
                    pass => 'TMVMI666',
                    parent => '/build_selfservice/TMVMI/6.0',
                    files => [
                        # module dir        sub dir1           file regrex1,  can include many sub dir and file regrex pair
                        ['Kernel',          'mui/output/RPMS',"kernel-*.rpm" ],
                        ['Libvirt',         'mui/output/RPMS',"libvirt-*.rpm" ],
                        ['TMVMI_Infra',     'mui/output/server/manager', "*.rpm"],
                        ['Unia',            'mui/output', "unia.tar.gz"],
                    ]
                );

die "ARGS: versionno   [updrepo|updlive|skipdown|onlyiso|updiso=newcentos.iso]\n" if( grep /^(help|h|\?)$/i,@ARGV );
       
my $version_number  = first {/^\d+$/} @ARGV;
my $version_major   = "6.0";

$version_number = '0001' if( !$version_number );

#   first run, uncomment these lines to download centos7 pkgs to local:
if( grep /^updrepo$/,@ARGV  ){
   init_centos7repo($vmiiso_pkgdir,$vmiiso_rpmcfg,$vmiiso_localrpmcfg);
   die "finish download centos pkgs\n";                
}

qx( env rm -rf $vmiiso_tmpdir) if( !(grep /^(updrepo|updlive|onlyiso)$/,@ARGV)  );
mkdir $vmiiso_tmpdir;

my $updiso = first {/^updiso=/} @ARGV;
if( $updiso && $updiso =~m/^updiso=(.*)/){
    my $isofile = $1;
    iso_updbase($isofile,$vmiiso_tmpdir,"$vmiiso_cfgdir/../template64");
    die "Finish update iso base to $isofile\n";
}

if( grep /^updlive$/,@ARGV  ){
    my $newrootfile = "$vmiiso_cfgdir/../template64/LiveOS/squashfs.img";
    iso_updlivefs("$vmiiso_tmpdir/newinst","$vmiiso_tmpdir/squash",$newrootfile);

    my $isorootfile = "./$vmiiso_tmpdir/iso/LiveOS/squashfs.img";

    if( -f $isorootfile ){
        print "Sync to $isorootfile\n";
        qx{env rm -f $isorootfile};
        qx{env cp -a $newrootfile $isorootfile};
    }
    die "finish repack exit \n" if( !(grep /^onlyiso$/,@ARGV)  );
}

print "version: $version_major-$version_number \nCentOS7 repo: $vmiiso_pkgdir\n";
if( !(grep /^(skipdown|onlyiso)$/,@ARGV)  ){
    redown_vmifiles($vmiiso_vmipkgdir);
}

if( !(grep /^onlyiso$/,@ARGV)  ){
    update_vmipkgs($vmiiso_vmipkgdir,$vmiiso_cfgdir,$vmiiso_tmpdir,$version_major,$version_number);
}
iso_prepare("../output/","./$vmiiso_tmpdir/iso/",$version_major,$version_number);
iso_geniso("../output/","./$vmiiso_tmpdir/iso/",$version_major,$version_number);

sub iso_geniso{
    my ($outputdir,$isodir,$version_major,$version_number) = @_;
    die "check dir $isodir fail\n" if( !-d $isodir);

    my $label = "TMVMI_$version_major.$version_number";
    my  $outputfile = "$outputdir/TMVMI-$version_major-$version_number.iso";
    my $geniso_param="-untranslated-filenames -V $label -J -joliet-long ";

    $geniso_param .= '-rational-rock -translation-table -input-charset utf-8 ';
    $geniso_param .= '-x ./lost+found -b isolinux/isolinux.bin -c isolinux/boot.cat -no-emul-boot ';
    $geniso_param .= '-boot-load-size 4 -boot-info-table -eltorito-alt-boot -e images/efiboot.img -no-emul-boot ';

    print "\n================= BEGIN GEN ISO FILE $outputfile ====================\n";
    qx($bin_genisoimage  $geniso_param -o $outputfile -T $isodir);
    die "gen iso file $outputfile fail\n" if( !-f $outputfile);
    qx($bin_checkisomd5 $outputfile);
    print "Finish gen iso file $outputfile\n"
}

sub iso_prepare{
    my ($outputdir,$isodir,$version_major,$version_number) = @_;
    my $version = "$version_major.$version_number";
    my $label = "TMVMI_$version_major.$version_number";
    my  $outputfile = "$outputdir/TMVMI-$version_major-$version_number.iso";

    die "centos 7 template not found\n"                     if( ! -d "./Product/config/template64/");
    die "vmi packages dir($vmiiso_vmipkgdir) not found\n"   if( ! -d $vmiiso_vmipkgdir);
    die "missing package: \n\tanaconda createrepo mkisofs rsync yum-utils genisoimage squashfs-tools\n" 
        if( !-f $bin_yumdownloader
            || ! -f $bin_rsync || !-f $bin_createrepo || !-f $bin_genisoimage || !-f $bin_checkisomd5);

    qx(env rm -rf $outputdir);
    mkdir($outputdir);
    qx(env rm -rf $isodir);
    mkdir $isodir;

    print "\n================= BEGIN PREPARE ISO DIR $isodir ====================\n";
    qx($bin_rsync -a --exclude=Packages/ --exclude=repodata/ ./Product/config/template64/ $isodir);
    mkdir "$isodir/Packages";
    mkdir "$isodir/repodata";
    sync_pkg2dst($vmiiso_pkgdir,"$isodir/Packages",$vmiiso_rpmcfg);
    sync_pkg2dst($vmiiso_vmipkgdir,"$isodir/Packages",$vmiiso_localrpmcfg);
    #qx($bin_cp -a $vmiiso_pkgdir/*.rpm          $isodir/Packages);
    #qx($bin_cp -a $vmiiso_vmipkgdir/*.rpm       $isodir/Packages);
    qx(env cp -a $vmiiso_cfgdir/ks.conf        $isodir/isolinux/);
    qx(env cp -a $vmiiso_cfgdir/isolinux.cfg   $isodir/isolinux/);
    die "$isodir/isolinux/isolinux.cfg not found\n" if ( ! -f "$isodir/isolinux/isolinux.cfg");
    replace_strings("$isodir/isolinux/ks.conf","TMVMI_VERSTR",$version_major);
    replace_strings("$isodir/isolinux/isolinux.cfg","VMI_VERLABEL",$version);
    replace_strings("$isodir/isolinux/isolinux.cfg","VMI_LABEL",$label);
    
    chown(0,0, "$isodir/isolinux/isolinux.cfg");
    chmod(0644,"$isodir/isolinux/isolinux.cfg");

    if( -d "$vmiiso_cfgdir/vmi" ){
        qx(env cp -a $vmiiso_cfgdir/vmi $isodir);
    }
    qx(env cp -a $vmiiso_cfgdir/comps.xml      $isodir/repodata/);
    die "$isodir/repodata/comps.xml not found\n" if ( ! -f "$isodir/repodata/comps.xml");
    my $curdir = getcwd;
    chdir ($isodir);
    qx($bin_createrepo  -g  repodata/comps.xml ./);
    chdir($curdir);
}

sub mnt_mount{
    my ($mntfile,$dstpoint,%cpops) = @_;
    die "file $mntfile not found\n" if (! -f $mntfile);
    qx( env umount -lf $dstpoint 2>/dev/null);
    mnt_recreatedir($dstpoint);
    qx( env mount $mntfile $dstpoint);
    while( my ($src,$dst) = each (%cpops)){
        print " mount exec $src => $dst\n";

        if( $src eq 'f' ){
            next if( -f $dst);

            mnt_umount($dstpoint);
            die "file $dst not found, maybe mount a wrong file $mntfile\n";
        }

        if( $src eq 'd' ){
            next if( -d $dst);

            mnt_umount($dstpoint);
            die "dir $dst not found, maybe mount a wrong file $mntfile\n";
        }

        if( (! -f $src) && (!-d $src) ){
            mnt_umount($dstpoint);
            die "file or dir $src not found, maybe mount a wrong file $mntfile\n";
        }
        qx( env rm -rf $dst);
        qx( env cp -a $src $dst);
    }
}

sub mnt_umount{
    my ($dst) = @_;
    qx( env umount -lf $dst);
    qx( env rm -rf $dst);
}

sub mnt_recreatedir{
    my ($dst) = @_;
    qx(env rm -rf $dst);
    qx(env mkdir -p $dst);
}

#   update base iso image, copy latest image files to $dstbase dir
sub iso_updbase{
    my ($isofile,$tmp,$dstbase) = @_;
    mnt_recreatedir($dstbase);

    my $isopoint = "$tmp/newiso/";
    mnt_mount($isofile,$isopoint,"f","$tmp/newiso/LiveOS/squashfs.img");

    print "begin update $dstbase\n";
    qx(env rsync -a --exclude=Packages/ --exclude=repodata/ $isopoint  $dstbase); 
    mnt_umount($isopoint);
    die "sync fail,dst file $dstbase/LiveOS/squashfs.img not found\n" if ( ! -f "$dstbase/LiveOS/squashfs.img" );

    print "begin update anaconda files\n";
    my $customdir="$dstbase/../anaconda";
    my $newroot="$tmp/newroot";
    my $pysrc="$newroot/usr/lib64/python2.7/site-packages/pyanaconda";
    my $pydst="$customdir/pyanaconda";
    my $uisrc="$newroot/usr/share/anaconda/ui";
    my $uidst="$customdir/ui";

    mnt_recreatedir($customdir);
    mnt_mount("$dstbase/LiveOS/squashfs.img","$tmp/newsq","f","$tmp/newsq/LiveOS/rootfs.img");
    mnt_mount("$tmp/newsq/LiveOS/rootfs.img",$newroot,
        $pysrc,$pydst,$uisrc,$uidst);
    mnt_umount($newroot);
    mnt_umount("$tmp/newsq");

    print "begin create custom anaconda files\n";
    qx(env cp -a $pydst/ui/categories/customization.py $pydst/ui/categories/tmvmi_settings.py);
    qx(env cp -a $pydst/ui/gui/spokes/password.py $pydst/ui/gui/spokes/vmipassword.py);
    qx(env cp -a $pydst/ui/gui/spokes/password.pyc $pydst/ui/gui/spokes/vmipassword.pyc);
    qx(env cp -a $pydst/ui/gui/spokes/network.py $pydst/ui/gui/spokes/vmiinstmode.py);
    qx(env cp -a $pydst/ui/gui/spokes/network.pyc $pydst/ui/gui/spokes/vmiinstmode.pyc);
    qx(env rm -f $pydst/ui/gui/spokes/user.py* );
    qx(env cp -a $uidst/spokes/password.glade $uidst/spokes/vmipassword.glade);
    qx(env cp -a $uidst/spokes/network.glade $uidst/spokes/vmiinstmode.glade);

    replace_strings("$pydst/ui/categories/tmvmi_settings.py","CustomizationCategory","TMVMISettingsCategory");
    replace_strings("$pydst/ui/categories/tmvmi_settings.py","CUSTOMIZATION","TRENDMICRO VMI SETTINGS");
    replace_strings("$uidst/spokes/vmipassword.glade","ROOT","TMVMI");
    replace_strings("$uidst/spokes/vmipassword.glade","root","tmvmi");
    replace_strings("$uidst/spokes/vmipassword.glade","id=\"","id=\"tmvmi");
    replace_strings("$uidst/spokes/vmiinstmode.glade","id=\"","id=\"tmvmi");
    #print "Finish update iso base to $isofile\n";
}

sub iso_updlivefs{
    my  ($outpoint,$squashpoint,$squashfsimg) = @_;
    my $mountpoint = "$vmiiso_tmpdir/squashtmp";

    #   check if squashfs-tools exists
    my $cmd = qx{whereis mksquashfs};
    die "cmd  mksquashfs not found, terminate\n" if( $cmd !~m/\/\w?bin\/mksquashfs/);

    print "prepare squashfs\n";
    mnt_recreatedir($squashpoint);
    mnt_mount($squashfsimg,$mountpoint,"f","$mountpoint/LiveOS/rootfs.img",
        $mountpoint,$squashpoint);
    mnt_umount($mountpoint);

    my $pysrc = "$vmiiso_cfgdir/../anaconda/pyanaconda";
    my $pydst = "$outpoint/usr/lib64/python2.7/site-packages/pyanaconda";
    my $uisrc = "$vmiiso_cfgdir/../anaconda/ui/spokes";
    my $uidst = "$outpoint/usr/share/anaconda/ui/spokes";
    print "prepare mount install image to $outpoint and update files\n";
    mnt_mount("$squashpoint/LiveOS/rootfs.img",$outpoint,
        $pysrc,$pydst,$uisrc,$uidst
    );
    qx(env chown -R root.root $pydst);
    qx(env chown -R root.root $uidst);
    mnt_umount($outpoint);

    qx(env rm -f $squashfsimg);
    qx(env mksquashfs $squashpoint $squashfsimg);
    print "Finish repace install image to $squashfsimg\n";
}

sub replace_strings{
    my  ($file,$orig,$new) = @_;
    if( open(my $fh,"$file") ){
        my @lines = <$fh>;
        close $fh;

        if( open(my $fh,">$file") ){
            foreach my $line (@lines){
                $line=~s/$orig/$new/g;
                print $fh $line;
            }
            close $fh;

            return 0;
        }
    }

    return 1;
}

#   param

#   API === file2rpmlist ===
#   convert rpm -qa or rpmfilelist result to rpm list
#   in a vmi server run: 
#       rpm -qa > rpms.txt
#   to get all rpms list(to resolve dependency)
#   then copy rpms.txt to cfg, run:
#       file2rpmlist('cfg/rpms.txt');
#   to remove cfg/rpms.txt version info
#
#   rpm -qa > cfg/rpms.txt
#file2rpmlist('cfg/rpms.txt');
#   find prerpm/ > cfg/rpms_local.txt
#file2rpmlist("cfg/rpms_local.txt");
#

sub pkgfile_get{
    my ($pkgpath,$pkgname) = @_;
    my $fname;
    my $tail;
    if( $pkgname!~m/(.*)(\.[^.]+)$/){
        die "pkg $pkgname unknown\n" if($pkgname!~m/^gpg-pubkey/);
        $fname=$pkgname;
        $tail='.rpm';
    }
    else{
        $fname=$1;
        $tail="$2.rpm";
    }
    return glob("$pkgpath/$fname-[0-9]*$tail");
}
sub sync_pkg2dst{
    my ($srcpath,$dstpath,$pkglist) = @_;

    if( open(my $fh,"$pkglist") ){
        my @localpkgs = <$fh>;
        close $fh;
        chomp(@localpkgs);

        foreach my $line (@localpkgs){
            my @localfile = pkgfile_get($srcpath,$line);
            foreach my $file (@localfile){
                copy $file, $dstpath;
            }
        }
    }
}

sub init_centos7repo{
    my ($dstpath,$pkglist,$locallist) = @_;
    mkpath($dstpath,1,0755);
    die "path $dstpath not exist\n" if( ! -d $dstpath );

    my $fh ;
    my @localpkgs;

    #   remove dst dir's local vmi pkg, it need to redownload every time
    if( open($fh,"$locallist") ){
        @localpkgs = <$fh>;
        close $fh;
        chomp(@localpkgs);

        foreach my $line (@localpkgs){
            my @localfile = pkgfile_get($dstpath,$line);
            foreach my $file (@localfile){
                unlink $file;
            }
        }
    }

    #   call yumdownlaod to down pkgs from centos repo
    open($fh,"$pkglist") or die "open file $pkglist to down pkgs fail\n";
    while(my $line=<$fh>){
        chomp($line);
        next if($line!~m/\w/);
        next if($line=~m/^gpg-pubkey/);
        if( grep /^$line$/,@localpkgs ){
            print "ignore local pkg $line\n";
            next;
        }

        my @files = pkgfile_get($dstpath,$line);
        next if( scalar(@files) > 0 );

        qx($bin_yumdownloader $line --downloadonly --destdir $dstpath);
        @files = pkgfile_get($dstpath,$line);
        print "glob  $line result: @files\n";
        die "down file $line fail\n" if( scalar(@files) < 1 );
    }
    close $fh;

    #   create localrepo
    qx(createrepo $dstpath);
}

sub file2rpmlist{
    my ($fname) = (@_);
    open my $fh,"$fname" or die "conv pkg list $fname open fail\n";
    my @pkgs=<$fh>;
    close $fh;

    chomp(@pkgs);
    s/\.rpm$//g for @pkgs;
    s/(.*\/)?(.*?)((-[0-9][^-]*)+)(\.[^.]+)$/$2$5/g for @pkgs;

    open $fh,">$fname" or die "conv pkg list $fname write fail\n";
    foreach my $line (@pkgs){
        print $fh "$line\n";
    }
    close $fh;
};

sub ftp_init{
    #my (%ftpinfo) = @_;
    my $f = Net::FTP->new($ftpinfo{host}) or die "Can't open $ftpinfo{host}\n";
    $f->login($ftpinfo{usr}, $ftpinfo{pass}) or die "Can't log $ftpinfo{usr} in\n";
    $f->cwd($ftpinfo{parent}) or die "Change to $ftpinfo{parent} fail\n";
    $f->binary();

    return $f;
}

sub ftp_getlatestver{
    my ($f,$name,@remoteinfo) = @_;

    my $parentdir = $f->pwd();

    my @dirs = $f->ls($name) or die "get $name version list fail\n";
    my $maxdir ;
        print "$name: @dirs\n";
    foreach my $item (@dirs){
        next if($item!~m/^\d+$/);
        $maxdir = $item if( !$maxdir || $item > $maxdir);
    }

    die "No version for $name in $parentdir\n" if( !$maxdir );
    
    while( @remoteinfo ){
        my $item  = shift @remoteinfo;
        my $regrex = shift @remoteinfo;
        my $path = "$parentdir/$name/$maxdir/$item";
        $f->cwd($path) or die "Change to $path fail\n";
        my @files =  $f->ls($regrex);
        foreach my $file (@files){
            $f->get($file) or die "get failed ", $f->message;
            print "  >> $path   -   $file\n";
        }
    }
    $f->cwd($parentdir) or die "Change to $parentdir fail\n";
    #print "dir is @dirs\n");

}

sub ftp_release{
    my ($f) = @_;
    $f->quit();
}


sub redown_vmifiles{
    my ($dst) = @_;
    
    qx(env rm -rf $dst);
    mkdir $dst;

    print "\n================= BEGIN GET LATEST VMI PACKAGES ====================\n";
    qx(env cp $vmiiso_cfgdir/../vmipkgs/*.rpm $dst);
    my $localolddir  = getcwd;
    chdir $dst or die "local change to $dst fail\n";
    my $ftp = ftp_init;
    foreach my $item  (@{$ftpinfo{files}}){
        ftp_getlatestver($ftp,@$item);
    }
    ftp_release($ftp);
    chdir $localolddir;
}

sub update_vmipkgs{
    my  ($vmidir,$cfgdir,$tmpdir,$releasever,$releasenum) = @_;

    print "\n================= BEGIN UPDATE LATEST VMI PACKAGES ====================\n";
    my $oldpath = getcwd;

    #   createe unia rpm
    if( -f "$vmidir/unia.tar.gz" ){
        print "repack unia\n";
        qx(env rm -rf unia/ $vmidir/unia-*.rpm ~/rpmbuild/SOURCES/unia.tar.gz ~/rpmbuild/BUILD/unia-* ~/rpmbuild/BUILDROOT/unia-*);
        qx(env mkdir -p unia/etc/ ~/rpmbuild/SOURCES/);
        qx(env cp    "$vmidir/unia.tar.gz" ~/rpmbuild/SOURCES/);
        qx(env cp    "$cfgdir/iptables" "unia/etc/");
        qx(echo "TMVMI release $releasever.$releasenum" > unia/etc/system-release);
        qx(/usr/bin/gunzip ~/rpmbuild/SOURCES/unia.tar.gz);
        qx(/usr/bin/tar -uf  ~/rpmbuild/SOURCES/unia.tar unia/etc/iptables unia/etc/system-release);
        qx(/usr/bin/gzip ~/rpmbuild/SOURCES/unia.tar);

#        rpmbuild --define "release_num 100" --define "release_ver 6.0" -bb Product/config/cfg/unia.spec
        qx(rpmbuild --define "release_num $releasenum" --define "release_ver $releasever" -bb $cfgdir/unia.spec);
        qx(env cp ~/rpmbuild/RPMS/x86_64/unia-*.rpm $vmidir/);
    }
    else{
        print "unia package not found, skip repack\n";
    }
}



