
#-----------------------------------------------------------------------------------------------------------------
#  pstree_getchildpids($parentpid)
#     get one parent pid's all child pid
#     return array item:  @{[name, pid]}
#
sub pstree_getchildpids()
{
  my ($ppid) = (@_);
  print "$ppid\n";

  my $str="pstree -p $ppid | sed 's/{.*//g' | sed 's/-/\\n/g' | egrep '[0-9]+' | sed 's/\\(.*\\)(\\([0-9]\\+\\).*/\\2 \\1/g'";
          #pstree -p 1     | sed 's/{.*//g' | sed 's/-/\n/g'  | egrep '[0-9]+' | sed 's/\(.*\)(\([0-9]\+\).*/\2 \1/g'
  print "str: $str\n";
  my @pids = qx($str);

  my @ret;
  foreach my $line (@pids){
     next if($line!~m/(\d+)\s+(.*)\s*/);
     my $pidinfo = [$2,$1];
     push @ret,$pidinfo;
  }
  return @ret;
}

sub pids_get_io()
{
  my @pids = @_;
  printf "%8s\t%8s\t%8s\t%4s\t%s\n",'total','rbytes','wbytes','pid','name';
  foreach my $line (@pids){
     my $pid=$$line[1];
     my $name=$$line[0];
     #print " $name = $pid \n";
     my $stat=qx!cat /proc/$pid/io |grep _bytes | sed 's/\\n/ /g'!;
     my $rbytes='N/A';
     my $wbytes='N/A';
     if( $stat=~m/read_bytes:\s+(\d+)\s*write_bytes:\s+(\d+)/s){
        $rbytes=$1;
        $wbytes=$2;
        next if( $rbytes == 0 &&  $wbytes == 0 && ($name!~m/libvirt_lxc/));
     }
     printf "%8d\t%8d\t%8d\t%4d\t%s\n",$rbytes+$wbytes,$rbytes,$wbytes,$pid,$name;
  }
  #print @pids;
}


#-----------------------------------------------------------------------------------------------------------------
#  pstree_getchildtids(pstree -p result file)
#      get pstree's all child process's tid info
#      return hash ref:  %{$procname,\@pids}
#
sub pstree_getchildtids{
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

    return \%procpidinfo;
}

#  convert pstree_getchildtids result to {$pid,$procname}
sub pstree_tids2tidhash{
    #   input %{$procname, @pids}
    #   output %{$pid,$procname}
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

#-----------------------------------------------------------------------------------------------------------------
