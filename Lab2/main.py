from flask import Flask, render_template

app = Flask(__name__, static_folder='static')

# 定义一个简单的路由，用于渲染网页
@app.route('/')
def hello_world():
    return render_template('index.html')

if __name__ == '__main__':
    app.run(port=8000)
