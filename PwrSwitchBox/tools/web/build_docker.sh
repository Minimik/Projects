cd flask-docker

if [[ "$1" == "build" ]]; then
    echo "Baue einen docker container aus einem hier liegenden dockerfile..."
    docker build -t flask-server .
    
elif [[ "$1" == "start" ]]; then
    echo "Starte den docker container..."
    docker run -d -p 5000:5000 --restart always --name flask-container flask-server
else
    echo "Verwendung: $0 {build|start}"
    exit 1
fi



