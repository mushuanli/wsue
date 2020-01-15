
BLOG_ROOT=$PWD/wordpress
BLOG_URI=musu.com
DB_ROOTPWD=root
DB_USR=rain
DB_PWD=rain

echo " info: https://www.digitalocean.com/community/tutorials/how-to-install-wordpress-with-docker-compose "
echo "$BLOG_ROOT"

function init_nginx(){
   mkdir -p $BLOG_ROOT/nginx-conf
    cat > $BLOG_ROOT/nginx-conf/nginx.conf <<EOF
    server {
        listen 80;
        listen [::]:80;

        server_name $BLOG_URI www.$BLOG_URI;

        index index.php index.html index.htm;

        root /var/www/html;

        # set client body size to 2M #
        client_max_body_size 2M;

        location ~ /.well-known/acme-challenge {
                allow all;
                root /var/www/html;
        }

        location / {
                try_files \$uri \$uri/ /index.php\$is_args\$args;
        }

        location ~ \.php\$ {
                try_files \$uri =404;
                fastcgi_split_path_info ^(.+\.php)(/.+)\$;
                fastcgi_pass wordpress:9000;
                fastcgi_index index.php;
                include fastcgi_params;
                fastcgi_param SCRIPT_FILENAME \$document_root\$fastcgi_script_name;
                fastcgi_param PATH_INFO \$fastcgi_path_info;
        }

        location ~ /\.ht {
                deny all;
        }

        location = /favicon.ico { 
                log_not_found off; access_log off; 
        }
        location = /robots.txt { 
                log_not_found off; access_log off; allow all; 
        }
        location ~* \.(css|gif|ico|jpeg|jpg|js|png)\$ {
                expires max;
                log_not_found off;
        }
} 
EOF
}

init_env(){
    cat > $BLOG_ROOT/.env <<EOF
MYSQL_ROOT_PASSWORD=$DB_ROOTPWD
MYSQL_USER=$DB_USR
MYSQL_PASSWORD=$DB_PWD
EOF

cat > $BLOG_ROOT/docker-compose.yml <<EOF
version: '3'

services:
  db:
    image: mysql:8.0
    container_name: db
    restart: unless-stopped
    env_file: .env
    environment:
      - MYSQL_DATABASE=wordpress
    volumes: 
      - dbdata:/var/lib/mysql
    command: '--default-authentication-plugin=mysql_native_password'
    networks:
      - app-network

  wordpress:
    depends_on: 
      - db
    image: wordpress:5.3.2-fpm-alpine
    container_name: wordpress
    restart: unless-stopped
    env_file: .env
    environment:
      - WORDPRESS_DB_HOST=db:3306
      - WORDPRESS_DB_USER=$DB_USR
      - WORDPRESS_DB_PASSWORD=$DB_PWD
      - WORDPRESS_DB_NAME=wordpress
    volumes:
      - wordpress:/var/www/html
    networks:
      - app-network

  webserver:
    depends_on:
      - wordpress
    image: nginx:1.15.12-alpine
    container_name: webserver
    restart: unless-stopped
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - wordpress:/var/www/html
      - ./nginx-conf:/etc/nginx/conf.d
      - certbot-etc:/etc/letsencrypt
    networks:
      - app-network

  certbot:
    depends_on:
      - webserver
    image: certbot/certbot
    container_name: certbot
    volumes:
      - certbot-etc:/etc/letsencrypt
      - wordpress:/var/www/html
    command: certonly --webroot --webroot-path=/var/www/html --email rain_li@musu.com --agree-tos --no-eff-email --staging -d $BLOG_URI -d www.$BLOG_URI

# located in /var/lib/docker/volumes/, and managed by host docker volume
volumes:
  certbot-etc:
  wordpress:
  dbdata:

# only visited in reference docker concainers, don't export to public
networks:
  app-network:
    driver: bridge  
EOF
}

case $1 in
    i)
	echo 'init'
	init_nginx
	init_env
	;;

    s|run)
	echo 'start'
	cd $BLOG_ROOT && docker-compose up -d && echo 'start wordpress success'
	;;

    t|stop)
        echo "stop $2:"
        cd $BLOG_ROOT && docker-compose stop $2 
        ;;

    r|restart)
        echo "restart $2:"
        cd $BLOG_ROOT && docker-compose up -d --force-recreate --no-deps $2
        ;;

    ps)
        echo "wordpress status:"
        cd $BLOG_ROOT && docker-compose ps 
        ;;

    logs)
        echo "logs for $2:"
        cd $BLOG_ROOT && docker-compose logs $2 
        ;;

    exec)
        echo "exec in $2:"
        cd $BLOG_ROOT && docker-compose exec $2 "$3" "$4" "$5" "$6" "$7" "$8"
        ;;

    *)
	echo "help:"
        echo "    wordpress in docker: will install a wordpress by docker, and export 80/443 port."
        echo "    the blog post saved in docker volume."
	echo "params:  "
        echo "  i|init:"
        echo "  s|run:"
        echo "  ps:"
        echo "  t|stop: [servername]"
        echo "  r|restart: [servername]"
        echo "  logs: [servername]"
        echo "  exec: [servername] command list"
	;;
esac

