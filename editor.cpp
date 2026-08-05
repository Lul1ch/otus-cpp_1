#include <iostream>
#include <string>
#include <memory>
#include <vector>

constexpr double PI_number = 3.14;

enum GraphicEditRequest
{
    GRAPH_NULL_REQ,
    DRAW_CIRCLE,
    DRAW_SQUARE,
    DRAW_TRIANGLE,
    ERASE_FIGURE,
    MOBE_FIGURE
};

enum TextEditRequest
{
    TEXT_NULL_REQ,
    ADD_TEXT,
    EDIT_TEXT,
    DELETE_TEXT
};

template <typename T>
class Dot
{
public:
    T getX() { return m_x; }
    T getY() { return m_y; }
private:
    T m_x;
    T m_y;
};

template <typename T>
class Figure
{
public:
    virtual void draw();
    virtual T getSquare();
private:
    int layer;
    T center;
};

template<typename T>
class Circle : public Figure<T>
{
public:
    void draw() override;
    T getSquare() override { return m_square; }
private:
    T radius;
    T m_square;
};

template <typename T>
class Square : public Figure<T>
{
public:
    void draw() override;
    T getSquare() override { return m_square; }
private:
    T side;
    T m_square;
};

template<typename T>
class Triangle : public Figure<T>
{
public:
    void draw() override;
    T getSquare() override { return m_square; }
private:
    T m_high;
    T side;
    T m_square;
};

class Field
{
public:

    virtual void updateFieldContent();
    virtual void loadFieldContent();
};

template <typename T>
class GraphicField : public Field
{
public:
    void updateFieldContent() override;
    void loadFieldContent() override;
    
    template <typename U>
    void drawFigure(const Figure<U>& figure);
    
    template <typename U>
    void eraseFigure(const Figure<U>& figure);
    
    template <typename U>
    void moveFigure(const Figure<U>& figure, Dot<U> new_coordinates);
    
    template <typename U>
    std::vector<Figure<U>>& getFiguresList() { return m_figures_vec; }

private:
    std::vector<Figure<T>> m_figures_vec;
};

class TextField : public Field
{
public:
    void updateFieldContent() override;
    void loadFieldContent() override;
    void editText(size_t substr_start, size_t substr_end, std::string new_substr);
private:
    std::string m_document_text;
};

template <typename T>
class ContentField
{
public:
//Graphic
    template <typename U>
    void drawCircle(Dot<U> center, U radius, int layer);

    template <typename U>
    void drawSquare(Dot<U> center, U side, int layer);

    template <typename U>
    void drawTriangle(Dot<U> center, U high, U side, int layer);

    template <typename U>
    void eraseFigure(Dot<U> dot);

    template <typename U>
    void moveFigure(Dot<U> dot);
//Text
    void editTextField(size_t substr_start, size_t substr_end, std::string text);
//General
    template <typename U>
    Figure<U>& findPropriateFigure(Dot<U> dot);
private:

    std::unique_ptr<TextField> m_text_field;
    std::unique_ptr<GraphicField<T>> m_graphic_field;
};

class Document
{
public:

    bool checkDocumentIsValid()
    {
        return true;
    }

    std::string const getPathToDocument()
    {
        return m_path_to_file;
    }

    template <typename U>
    TextField* const loadGraphicFieldFromDocument()
    {
        return new TextField();
    }

    template <typename U>
    GraphicField<U>* const loadGraphicFieldFromDocument()
    {
        return new GraphicField<U>();
    }

    template <typename U>
    ContentField<U>* const loadContentFieldFromDocument()
    {
        return new ContentField<U>();
    }

private:

    std::string m_path_to_file;

};

struct UserRequest
{
    size_t request_number;
};

struct UserTextEditRequest : public UserRequest
{
    size_t substr_start;
    size_t substr_end;
    std::string text = "";
};

template <typename T>
struct UserGraphicEditRequest : public UserRequest
{
    Dot<T> dot;
};

template <typename T>
class TextEditor
{
public:
    void processTextEditRequest(UserRequest& req);
    void loadTextPart(const std::shared_ptr<Document>& doc) 
    {
        if (doc->checkDocumentIsValid())
        {
            /*
                Загружаем контент из файла
                m_content_field = std::make_shared<ContentField<T>>(doc->loadContentFieldFromDocument());
            */
        }
    }
private:

    std::shared_ptr<ContentField<T>> m_content_field;
};

template <typename T>
class GraphicEditor
{
public:
    void processGraphicEditRequest(UserRequest& req);
    void loadGraphicPart(const std::shared_ptr<Document>& doc) 
    {
        if (doc->checkDocumentIsValid())
        {
            /*
                Загружаем контент из файла
                m_content_field = std::make_shared<ContentField<T>>(doc->loadContentFieldFromDocument());
            */
        }
    }
private:

    std::shared_ptr<ContentField<T>> m_content_field;
};

class DocumentLoader
{
public:
    std::shared_ptr<Document> createEmptyDocument(const std::string& path_to_file)
    {
        /* Создаём файл по пути переданному в качестве аргумента и возвращаем его */

        return m_current_document;
    }

    std::shared_ptr<Document> loadDocument(const std::string& path_to_file)
    {
        /* Проверяем, что файл существует и валидного расширения. Загружаем файл */

        return m_current_document;
    }

    void saveDocument()
    {

    }

private:

    std::shared_ptr<Document> m_current_document;
};

class inputReader
{
public: 
    UserRequest* tryGetUserRequest()
    {
        return nullptr;
    }

    bool isTextFieldEditing()
    {
        return m_is_text_field_editing;
    }
private:
    bool m_is_text_field_editing; // отслеживаем, когда пользователь редактирует текст, если флаг поднят, то в это время фигуру рисовать он не может
};

class UIManager
{
public:
    void proccessUI()
    {
        //convertToUserRequest();
    }

    void convertToUserRequest()
    {

    }
};

template <typename T>
class Program
{
public:
    void mainThread()
    {
        std::unique_ptr<inputReader> input_reader;

        while(true)
        {
            UserRequest* req = input_reader->tryGetUserRequest();
            if (req != nullptr)
            {
                if (input_reader->isTextFieldEditing())
                {
                    //m_text_editor->processTextEditRequest(req);
                }
                else
                {
                    //m_graphic_editor->processGraphicEditRequest(req);
                }
            }
            updatePeriodic();
            // sleep for several miliseconds
        }
    }

    void generalUIThread()
    {
        std::unique_ptr<UIManager> ui_manager;
        while(true)
        {
            ui_manager->proccessUI();
            // sleep for several miliseconds
        }
    }

    void openFile(std::string path_to_file)
    {
        std::shared_ptr<Document> doc = m_document_loader->loadDocument(path_to_file);
        m_text_editor->loadTextPart(doc);
        m_graphic_editor->loadGraphicPart(doc);
    }

    void createEmptyFile(std::string path_to_file)
    {
        std::shared_ptr<Document> doc = m_document_loader->createEmptyDocument(path_to_file);
        m_text_editor->loadTextPart(doc);
        m_graphic_editor->loadGraphicPart(doc);
    }
private:
    std::unique_ptr<TextEditor<T>> m_text_editor;
    std::unique_ptr<GraphicEditor<T>> m_graphic_editor;
    std::unique_ptr<DocumentLoader> m_document_loader;

    void updatePeriodic()
    {
        m_document_loader->saveDocument();
    }
};

int main()
{
    std::cout << "This is a document editor template\n";
    return 0;
}