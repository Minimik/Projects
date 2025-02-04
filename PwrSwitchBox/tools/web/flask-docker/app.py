from flask import Flask

app = Flask(__name__)

@app.route("/")
def home():
    return "Hallo, das ist mein Flask-Server auf dem Raspberry Pi!"

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)