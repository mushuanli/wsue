#!/usr/bin/perl

#   author: mushuanli@163.com/lizlok@gmail.com, a tool use to parse strace output
#   normally parse this command output:
#       '(/opt/bin/sniff -i eth0 -F \'TCP{PORT!389}\' -w 050413.cap  &); strace -s 300 -f -ff -o aa -tt  -p \"`pidof libvirtd`\";pkill -SIGINT sniff ; pstree -p >tree'
#   the program will:
#   1.  parse ARGV[0] file (parsetree_exec) to generate a {$procname,\@pids} hash.current the procname only include libvirtd and libvirt_lxc's child process(@config_parentprocname)
#   2.  merge process's all task trace output into single file and sort it
#   3.  convert file handle to filename, and put it to "$ARGV[1]/fd"
#   4.  use clean command to filter "$ARGV[1]/fd"'s all file, and output to "$ARGV[1]/fd/clear/"
use Cwd;
use strict;
use POSIX qw(tmpnam);

#
#   configure
#
#   1.  only trace these process and it's child process
my  @config_parentprocname=('libvirtd','libvirt_lxc');

#   3.  will merge one process's all tid trace file into single file, filename format:
#my  $cleancmd=q{ egrep '(sendto|send|recv|recvfrom|read|write|connect|bind|listen|accept|execve|fork|vfork|clone)\\(' | grep -vw tasks | grep -vw binder };
my  $cleancmd=q{ egrep -w '(sendto|send|recv|recvfrom|inotify|unlinkat|read|write|connect|bind|listen|accept|accept4|mount|umount2|execve|fork|vfork|kill|clone|token)' | grep -vw tasks | grep -vw binder };
my  $keycmd=q{ egrep -w '(SENDTO|RECVFROM|READ|WRITE|UNLINK|CLONE|EXEC|ACCEPT|LISTEN|token|MOUNT|UMOUNT)' };


#
#   help
#
print "strace_parse.pl pstree_file strace_file\n";
print "\tuse to parse this command:\n";
print "\t\t".'(sniff -i eth0 -P TCP -w 050413.cap  &); strace -s 160 -f -y -yy -tt  -p "`pidof libvirtd`" -o ;pkill -SIGINT sniff ; pstree -p >tree'."\n";

die "pstree_file not found \n" if (! -f $ARGV[0]);
die "strace_file not found \n" if (! -f $ARGV[1]);

my  $pids   = parsetree_exec($ARGV[0]);


my $pidhash = parsetree_2pidhash($pids);
#parsetree_dump($pids);

my $cleanname = "clean_$ARGV[1]";
my $tmpname   = "tmp_$ARGV[1]";
my $tmpname2  = "tmp2_$ARGV[1]";
my $keyfname  = "key_$ARGV[1]";


genoutput_mergesyscall($ARGV[1],$tmpname2);
system( " cat $tmpname2 |$cleancmd > $tmpname");

my $sockmap = genoutput_getsocketmap($tmpname);
genoutput_getcleanfile($tmpname,$cleanname,$pidhash,$sockmap);
unlink($tmpname);
system( " cat $cleanname |$keycmd > $keyfname");

#   replace pid to
unlink($tmpname2);












sub genoutput_replacepids{
    my ($iname,$pids) = @_;
}

sub genoutput_mergesyscall{
    my ($inname,$outname) = @_;
    open (my $fin,'<',$inname) or die "open file $inname to parse fd fail\n";
    open (my $fout,'>',$outname) or die "open file $outname to write fd fail\n";

    my %res;
    while( my $line = <$fin> ){
        if( $line=~m/^\s*(\d+)(\s+\d+:\d+:\d+\.\d+\s+)(.*)\s+<unfinished ...>\s*$/ ){
            $res{$1}=$3;
            print $fout "$1$2\n";
            #print "===$res{$1}\n";
            next;
        }
        else{
            if( $line=~m/^\s*(\d+)(\s+\d+:\d+:\d+\.\d+\s+)<...\s+\w+\s+resumed>\s*(.*)$/ ){
                if( $res{$1} ){
                    print $fout "$1$2$res{$1}$3\n";
                    #print "=+=$1$2$res{$1}$3\n";
                    undef $res{$1};
                    next;
                }
            }
        }

        print $fout "$line";
    }

    close($fin);
    close($fout);
}


sub genoutput_getsocketmap{
    #   input  strace_filename
    #   output %{sockinfo,unixpath}
    my ($fname) = @_;
    my @lines = qx(grep -w connect $fname| grep -w AF_UNIX);
    my %res;
    foreach my $line (@lines){
        my $name;
        my $value;
        if( $line=~m/connect\((\d+<(socket|UNIX):\[\d+\]>), \{sa_family=AF_UNIX, sun_path=@?\"([^"]+)\"\}/ ){
            $name   = $1;
            $value  = $3;
        }
        else{
            die "find unknown unix socket:\n\t $line\n";
        }

        #print "$name - [$value] \n";
        if( $value=~m/\/var\/run\/nscd\/socket/ ){
            $value = "nscd_socket";
        }
        else{
            $value=~s/\/dev\/socket\///;
        }
        $res{$name} = $value;
        #print "$name - $value \n";
    }

    return \%res;
}

sub parseline{
    my ($line,$sockmap) = @_;

    if($line=~m/^\s*(sendto|read|write)\(([^ ]+),\s*(\".*\{\\\"type\\\":3,\\\"results\\\":.*)/ ){
        my  $cmd=uc $1;
        my $sock=$2;
        my $param=$3;
        $param=~s/\.\.\.,\s\w+//g;
        $param=~s/,\s\w+//g;
        $param=~s/\\\"/\"/g;
        return sprintf("%-28s%s    %s",$cmd,$sock,$param);
    }

    if($line=~m/^\s*unlinkat\(\s*\w+\s*,\s*\"(\.bootok\..*)\".*=\s*(\d+)/ ){
        return sprintf("%-28s%-52s= %s","UNLINK",$1,$2);
    }
    if($line=~m/^(\s*sendto\()([^,]+),(.*)/ ){
        if( !$$sockmap{$2} ){
            return $line;
        }

        if($$sockmap{$2} ne 'property_service' ){
            return "$1$2$$sockmap{$2},$3";
        }

        my $tail =  $3;
        $tail=~s/\"\\\d\\0\\0\\0//g;
        $tail=~s/\\0/ /g;
        $tail=~s/\s+"\.*,\s\w+,\s\w+,\s\w+,\s\w+\s*\)/ /g;
        my @array=split /\s*=\s*/,$tail;
        return sprintf("SENDTO   property_service  %-52s = %s",$array[0],$array[1]);
    }

    if($line=~m/^(\s*recvfrom\()(.*),\"\/dev\/socket\/property_service\"\]\>,\s+(.*)/ ){
        my $tail  = $3;
        $tail=~s/\"\\\d\\0\\0\\0//g;
        $tail=~s/\\0/ /g;
        $tail=~s/\s+"\.*,\s*\w+,\s*\w+,\s*\w+,\s*\w+\s*\)/ /g;
        my @array=split /\s*=\s*/,$tail;
        return sprintf("RECVFROM property_service   %-52s= %s",$array[0],$array[1]);
    }
    if($line=~m/^\s*clone\(.*CLONE_THREAD.*\)(\s+=\s*\d+\s*)$/){
        return sprintf("%-79s%s","CLONE_Thread",$1);
    }

    if($line=~m/^\s*clone\(.*\)(\s+=\s*\d+\s*)$/){
        return sprintf("%-79s%s","CLONE",$1);
    }
    if($line=~m/^\s*execve\(\s*\"[^\"]*",\s*(.*)\)(\s+=\s*\d+\s*)$/){
        return "EXEC                        $1$2";
    }
    if($line=~m/^\s*listen\((.*)\)\s+=\s*(\d+)\s*$/){
        return sprintf("LISTEN                      %-52s= %s",$1,$2);
    }
    if($line=~m/^\s*accept4\(.*TCP.*\)(\s+=.*)/){
        return "ACCEPT                      $1";
    }
    if($line=~m/^\s*mount\(\s*\"([^"]*)\",\s*\"([^"]*)\".*(\s+=\s*\d+)/ ){
        return "MOUNT                      $1    $2$3";
    }
    if($line=~m/^\s*umount2\(\s*\"([^"]*)\".*(\s+=\s*[0-9\-]+)/ ){
        return "UMOUNT                     $1$2";
    }
    return $line;
}

sub genoutput_getcleanfile{
    my ($infile,$outfile,$pidarray,$sockmap) = @_;
    open (my $fin,'<',$infile) or die "open file $infile to parse fd fail\n";
    open (my $fout,'>',$outfile) or die "open file $outfile to write fd fail\n";

    while( my $line = <$fin> ){
        my $newline = parseline($line,$pidarray,$sockmap);
        my ($title,$body);
        if( $line=~m/^\s*(\d+)\s+(\d+:\d+:\d+\.\d{3})\d*\s+(.*)/){
            my ($pid,$time,$cmd);
            $pid    = $1;
            $time   = $2;
            $cmd    = $3;

            if( $$pidarray{$pid} ){
                $title=sprintf("%s%16s<%6d> ",$time,$$pidarray{$pid},$pid);
            }
            else{
                $title=sprintf("%s%16s<%6d> ",$time,"",$pid);
            }

            $body=parseline($cmd,$sockmap);
        }
        else{
            die "unknown line my -$line-\n";
        }

       print   $fout  "$title$body\n";
    }

    close($fin);
    close($fout);
}


sub replacefd_exec{
    my  @files  = @_;
    foreach my $fname (@files){
        replacefd_1file($fname);
    }
}

sub replacefd_1file{
    my  $fname      = shift ;

    my  $foutname   = $fname;
    $foutname       =~ s/(.*)(\\|\/)(.*?)\.txt$/\1\/fd\/\3\.txt/g;
    my  $cleandir   = $fname;
    $cleandir      =~ s/(.*)(\\|\/)(.*?)\.txt$/\1\/fd\/clear\//g;
    my  $cleanname  = $fname;
    $cleanname      =~ s/(.*)(\\|\/)(.*?)\.txt$/$cleandir\3\.txt/g;

    my  @fds;
    #   print   "$fname -> $foutname\n";

    open (my $fin,'<',$fname) or die "open file $fname to parse fd fail\n";
    open (my $fout,'>',$foutname) or die "open file $foutname to write fd fail\n";
    while( my $line = <$fin> ){
        if( $line !~ m/^(\d+:\d+:\d+\.\d+\s+\<\d+\#\w+\>:\s+)(\w*(open|read|write|bind|connect|send|recv|ioctl|close|getpeername)\w*)\((.*)/){
            print   $fout   "$line";
            next;
        }

        my  $title  = $1;
        my  $func   = $2;
        my  $key    = $3;
        my  $rest   = $4;

        if( $func =~ m/(read|write|send|recv|ioctl)/ ){
            if( $rest =~ m/^(\d+),(.*)/ ){
                my  $fd         = $1;
                my  $funcrest   = $2;
                if( $fds[$fd] ){
                    print   $fout   "$title$func\($fd<$fds[$fd]>,$funcrest\n";
                    next;
                }
            }

            print   $fout   "$title$func\($rest\n";
            next;
        }

        if( $func =~ m/close/ ){
            if( $rest =~ m/^(\d+)\)(.*)/ ){
                my  $fd         = $1;
                my  $funcrest   = $2;
                if( $fds[$fd] ){
                    print   $fout   "$title$func\($fd<$fds[$fd]>\)$funcrest\n";
                    undef $fds[$fd];
                    next;
                }
            }

            print   $fout   "$title$func\($rest\n";
            next;
        }

        #open|bind|connect|getpeername
        if($func eq 'openat' ){
            #openat(AT_FDCWD, "/proc/self/task", O_RDONLY|O_LARGEFILE|O_DIRECTORY) = 11
            if( $rest =~ m/^\w+, \"[^"]+(\\|\/)([^"]+)\".*= (\d+)$/ ){
                my  $fd     = $3;
                my  $name   = $2;

                print "<$fname:$title>duplicate fd $fd, old name $fds[$fd] new name $name " if( $fds[$fd] );

                $fds[$fd]   = $name;
                #print   "$func  - $fd - $name\n";
            }

            print   $fout   "$title$func\($rest\n";
            next;
        }

        if( $func =~ m/(bind|connect)/ ){
            if( $rest =~ m/^(\d+),.*sun_path=\"[^"]+(\\|\/)([^"]+)\"/ ){
                my  $fd     = $1;
                my  $name   = $3;
                die "<$fname:$title>duplicate fd $fd, old name $fds[$fd] new name $name " if( $fds[$fd] );

                $fds[$fd]   = $name;
                #print   "$func  - $fd - $name\n";
            }

            print   $fout   "$title$func\($rest\n";
            next;
        }

        if( $func =~ m/getpeername/ ){
            if( $rest =~ m/^(\d+),.*sin_port=htons\((\d+)\), sin_addr=inet_addr\(\"([0-9.]+)\"\)/ ){
                my  $fd     = $1;
                my  $name   = "$3:$2";

                die "<$fname:$title>duplicate fd $fd, old name $fds[$fd] new name $name " if( $fds[$fd] );
                $fds[$fd]   = $name;
                #print   "$func  - $fd - $name\n";
            }

            print   $fout   "$title$func\($rest\n";
            next;
        }

        #print "$title   - $func - $rest \n";
    }

    close $fin;
    close $fout;

    system( "head -n1 $foutname > $cleanname");
    system( " cat $foutname |$cleancmd >> $cleanname");
}


sub generate_summary{
    my  ($dir)  = @_;
    my $curdir=getcwd();
    chdir($dir);

#   generate property_service time
    unlink("property_service");
    unlink("process.tmp");
    unlink("index");

    my $cmd=q{egrep 'sendto\([0-9]+<property_service>,' bb.* \
    | sed -e 's/sendto([0-9]\+<property_service>, \"\\\\[0-9]\\\\0\\\\0\\\\0/    /' \
    | sed -e 's/\\\\0/ /g'| sed -e 's/\".*//' \
    | sed -e 's/^bb\.\(.*\)\.txt:\([0-9]\+:[0-9]\+:[0-9]\+\.[0-9]\+\) <\([0-9]\+\).*>:/\2 \1:\3    /' | sort > property_service};
    print "$cmd\n";
    qx($cmd);

    #   generate process execve/bind/listen/accept time
    $cmd=q{egrep -w 'vfork|fork|clone' bb.* | grep -v '|CLONE_THREAD|' \
    | sed -e 's/^bb\.\(.*\)\.txt:\([0-9]\+:[0-9]\+:[0-9]\+.[0-9]\+\) <\([0-9]\+\).*>: \([a-z]\+\).*= \([0-9]\+\)/\2 \1:\3  \4  = \5/' > process.tmp};
    print "$cmd\n";
    qx($cmd);
    $cmd=q{egrep -w 'listen|accept' bb.* \
    | sed -e 's/^bb\.\(.*\)\.txt:\([0-9]\+:[0-9]\+:[0-9]\+.[0-9]\+\) <\([0-9]\+\).*>: \([a-z]\+\)(\([0-9]\+\), .*= \([0-9]\+\)/\2 \1:\3  \4   \5/' >> process.tmp};
    print "$cmd\n";
    qx($cmd);
    $cmd=q{egrep -w 'bind' bb.* | sed -e 's/bind(\([0-9]\+\), /bind \1 /'  \
    | sed -e 's/\{sa_family=AF_INET6, sin6_port=htons(\([0-9]\+\)),.* = \([0-9]\+\)/ INET6 \1/' \
    | sed -e 's/\{sa_family=.*\"\(.*\)\".* = \([0-9]\+\)/\1/' \
    | sed -e 's/\{sa_family=AF_NETLINK, pid=\([0-9]\+\), groups=\([0-9a-zA-Z]\+\).* = \([0-9]\+\)/ NETLINK \1,\2/g' \
    | sed -e 's/^bb\.\(.*\)\.txt:\([0-9]\+:[0-9]\+:[0-9]\+.[0-9]\+\) <\([0-9]\+\).*>:/\2 \1:\3 /' >> process.tmp};
    print "$cmd\n";
    qx($cmd);

    $cmd=q{egrep -w execve bb.*  \
    | sed -e 's/ execve(\"\([^"]\+\)\".*/    EXEC \1/' \
    | sed -e 's/^bb\.\(.*\)\.txt:\([0-9]\+:[0-9]\+:[0-9]\+.[0-9]\+\) <\([0-9]\+\).*>:/\2 \1:\3/' >> process.tmp};
    print "$cmd\n";
    qx($cmd);
    $cmd=q{cat property_service process.tmp \
    | sort  |awk ' { printf "%s\\t%-20s\\t%-10s\\t%s\\t%s\\t%s\\t%s\\t%s\\n",$1,$2,$3,$4,$5,$6,$7,$8}' > index };
    print "$cmd\n";
    qx($cmd);
    unlink("process.tmp");
    chdir ($curdir);
}











#   convert hash{name} -> \array[pids] to hash{pid} -> $name
sub parsetree_2pidhash{
    #   input %{$procname, @pids}
    #   output %{$pid,$procnmae}
    my  %pids   = %{$_[0]};
    my  %res;
    foreach my $key (keys %pids){
        my  @arr    = @{$pids{$key}};
        foreach my $value (@arr){
            $res{$value} = $key;
        }
    }

    return \%res;
}

sub parsetree_dump{
    my  %pids   = %{$_[0]};
    foreach my $key (keys %pids){
        my  @arr    = @{$pids{$key}};
        print "$key - @arr\n";
    }
}

#   simple  mode, use hash array:
#       name    item    level   lineno

sub parsetree_analyselinepart{
    my  ($level,$resref,$lineno,$line,$offset,$end)    = @_;

    my  $part;

    if( $end == -1 ){
        $part       = substr($line,$offset);
    }
    else{
        $part       = substr($line,$offset, $end-$offset);
    }

    my  @words  = split(/-+/,$part);
    my  $olditem;

    foreach my $word (@words){
        next if( $word !~ m/((\{?).+\}?)\((\d+)\)/);

        my  $item       = {};
        $item->{name}   = $1;
        $item->{pid}    = $3;
        if( $2 eq '{' && $olditem ){
            $item->{level}  = $level+1;
        }
        else{
            $item->{level}  = $level;
        }
        $item->{lineno} = $lineno;
        #print "$item->{lineno} - $item->{name} - $item->{pid} - $item->{level} \n";
        push @$resref,$item;
        $olditem    = $item;
    }
}

sub parsetree_analyseline{
    my  ($lineno,$line,$resref,$stackref)  = @_;

    my  $stacksz    = scalar(@$stackref);
    my  $i          = $stacksz -1;

    #   parse stack first,
    #   if any character before stack[i], then means this item invalid
    for( ; $i > -1; $i -- ){
        my  $firstpart  = substr($line,0,$$stackref[$i]);
        if( $firstpart =~ m/\w/ ){
            pop @$stackref;
        }
        else{
            last;
        }
    }

    $i++;

    my $offset = 0;
    my $result = index($line, '+', $offset);
    while ($result != -1) {
        parsetree_analyselinepart($i,$resref,$lineno,$line,$offset,$result);

        $i++;
        push @$stackref,$result;

        $offset = $result + 1;
        $result = index($line, '+', $offset);
    }
    parsetree_analyselinepart($i,$resref,$lineno,$line,$offset,$result);
}


sub parsetree_pushpid{
    my  ($out,$name,$item)    = @_;

    #print "push $name,$item->{pid}\n";
    if( !$out->{$name} ){
        my  @arr    = $item->{pid};
        $out->{$name} = \@arr;
    }
    else{
        push @{$out->{$name}},$item->{pid};
    }

}

#   return hash{$name} = \array[pid list]
sub parsetree_exec{
    my  $fname  = shift;
    my   $fh;
    open ($fh,'<',$fname) or die "open pstree_file <$fname> fail\n";


    my  %procpidinfo;



    my  @res;
    my  @stack;
    my  $i  = 1;

    while(my $line = <$fh> ){
        parsetree_analyseline($i++,$line,\@res,\@stack);
    }
    close $fh;

    my  $keyword;
    my  $start_line         = 0;
    my  $start_level        = 0;

    undef   @stack;
    my      $ressz  = scalar(@res);
    my      $lastitem;

    foreach my $pidinfo (@res) {

        if( $keyword ){
            if( $pidinfo->{lineno} != $start_line
                && $pidinfo->{level} <= $start_level ){
                undef $keyword;
            }
        }

        if( !$keyword ){
            next if( !($pidinfo->{name} ~~ @config_parentprocname) );
            $keyword        = $pidinfo->{name};
            $start_line     = $pidinfo->{lineno};
            $start_level    = $pidinfo->{level};
            $lastitem       = $pidinfo;
            parsetree_pushpid(\%procpidinfo,$pidinfo->{name},$pidinfo);
            next;
        }




        #print "<$keyword:$start_level>$pidinfo->{lineno} - $pidinfo->{name} - $pidinfo->{pid} - $pidinfo->{level} \n";
        if( index($pidinfo->{name},'{') == 0 ){
            while( $pidinfo->{level} <= $lastitem->{level} ){
                $lastitem   = pop @stack;
                #print "1after pop, $pidinfo->{level} < $lastitem->{level},$lastitem->{name} \n";
            }

            parsetree_pushpid(\%procpidinfo,$lastitem->{name},$pidinfo);
            next;
        }


        if( $pidinfo->{level} > $lastitem->{level} ){
            #   if is child process, push
            push @stack,$lastitem;
            #print "push $lastitem->{level},$lastitem->{name}\n";
            $lastitem   = $pidinfo;

            parsetree_pushpid(\%procpidinfo,$pidinfo->{name},$pidinfo);

            next;
        }

        if( $pidinfo->{level} == $lastitem->{level} ){
            $lastitem   = $pidinfo;
            parsetree_pushpid(\%procpidinfo,$pidinfo->{name},$pidinfo);
            next;
        }

        while( $pidinfo->{level} < $lastitem->{level} ){
            $lastitem   = pop @stack;
            #print "2after pop, $pidinfo->{level} < $lastitem->{level},$lastitem->{name} \n";
        }
        $lastitem   = $pidinfo;
        parsetree_pushpid(\%procpidinfo,$pidinfo->{name},$pidinfo);
    }

=item
        my  $firstfind  = 0;

        if( $keyword_mode == 0 ){
            next if($keyword_mode == 0 && ($line !~m/(libvirt_lxc|libvirtd)/));

            if( $1 eq 'libvirt_lxc' ){
                $keyword_mode               = 1;
            }
            else{
                $keyword_mode               = 2;
            }

            $firstfind  = 1;
        }


        #   ▒▒▒▒ǵ▒һ▒У▒▒▒▒ж▒▒Ƿ▒▒▒▒▒һ▒▒ؼ▒▒ֵĿ▒ʼ
        if( !$firstfind ){
            my  $firstpart  = substr($line,0,$keyword_treepos);
            if( $firstpart =~ m/\w/ ){
                if( $line =~ m/(libvirt_lxc|libvirtd)/ ){
                    my  $match = $1;
                    if( $keyword_mode == 2 && ($match eq 'libvirt_lxc') ){
                        $keyword_mode           = 1;
                        $firstfind              = 1;
                    }
                    else{
                        if( $keyword_mode == 1 && ($match eq 'libvirtd') ){
                            $keyword_mode       = 2;
                            $firstfind          = 1;
                        }
                    }
                }

                if( !$firstfind){
                    $keyword_mode   = 0;
                    next;
                }
            }
        }

        my  $key;
        my  @words  = split(/-+/,$line);
        shift @words while( $words[0] !~ m/\(\d+\)/ );  #   skip not match line


        if( $firstfind ){
            $keyword_treepos    = index($line,'+');
            undef $subkeyword_name;
            $subkeyword_treepos = 0;

            if( $keyword_mode == 1 ){
                my  $subword    = 0;
                foreach my $word (@words){
                    if( $word eq '+' ){
                        $subword    = 1;
                        next;
                    }

                    next if( $word !~ m/(\{?.+\}?)\((\d+)\)/);
                    if( $subword < 2 ){
                        $key        = $1;
                        $subword ++ if($subword == 1);
                    }

                    my  @oldarr;
                    my  $pid        = $2;
                    if($procpidinfo{$key}){
                        @oldarr = @{$procpidinfo{$key}} ;
                    }
                    else{
                        undef @oldarr;
                    }
                    push @oldarr,$pid;
                    $procpidinfo{$key}  = \@oldarr;
                    #print "$key - @oldarr\n";
                }

                $keyword_mode   = 0 if( $keyword_treepos <= 0 );
                next;
            }
        }

        if( $keyword_mode == 2 ){
            my  @oldarr;
            if($procpidinfo{libvirtd}){
                @oldarr = @{$procpidinfo{libvirtd}} ;
            }
            else{
                undef @oldarr;
            }

            foreach my $word (@words){
                next if( $word !~ m/\{?.+\}?\((\d+)\)/);
                push @oldarr,$1;
            }
            $procpidinfo{libvirtd}  = \@oldarr;

            #print "libvirtd - @oldarr\n";

            $keyword_mode   = 0 if( $keyword_treepos <= 0 );
            next;
        }

        #   keyword_mode    == 1 && ! $firstfind
        my  @oldarr;
        undef $key;
        next if($words[0] !~ m/(\{?.+\}?)\((\d+)\)/);
        $key=$1;

        if( $subkeyword_treepos <= 0 ){
            $subkeyword_treepos         = index($line,'+');
            $subkeyword_name            = $key;
        }
        else{
            my  $firstpart                  = substr($line,0,$subkeyword_treepos);
            if( $firstpart =~ m/\w/ ){
                #print "-$subkeyword_treepos-$firstpart:$line";
                $subkeyword_treepos         = index($line,'+');
                if( $subkeyword_treepos > 0 ){
                    $subkeyword_name    = $key;
                }
            }
            else{
                $key    = $subkeyword_name;
            }
        }

        if($procpidinfo{$key}){
            @oldarr = @{$procpidinfo{$key}} ;
        }
        else{
            undef @oldarr;
        }

        foreach my $word (@words){
            next if( $word !~ m/\{?.+\}?\((\d+)\)/);
            push @oldarr,$1;
        }

        $procpidinfo{$key}  = \@oldarr;
        #print "$key - @oldarr\n";

        #print "word is<$keyword_mode>: @words\n";
    }

    close $fh;

    #print %procpidinfo;
=cut
    return \%procpidinfo;
}
