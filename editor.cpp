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
    MOVE_FIGURE
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

    virtual ~Figure() {}
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

    ~Circle() override {}
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

    ~Square() override {}
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

    ~Triangle() override {}
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

    virtual ~Field() {}
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
    
    ~GraphicField() override {}

private:
    std::vector<std::unique_ptr<Figure<T>>> m_figures_vec;
};

class TextField : public Field
{
public:
    void updateFieldContent() override;
    void loadFieldContent() override;
    void editText(size_t substr_start, size_t substr_end, std::string new_substr);

    ~TextField() override {}
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

    std::shared_ptr<TextField> loadTextFieldFromDocument()
    {
        return std::make_shared<TextField>();
    }

    template <typename U>
    std::shared_ptr<GraphicField<U>> loadGraphicFieldFromDocument()
    {
        return std::make_shared<GraphicField<U>>();
    }

    template <typename U>
    std::shared_ptr<ContentField<U>> loadContentFieldFromDocument()
    {
        return std::make_shared<ContentField<U>>();
    }

private:

    std::string m_path_to_file;

};

struct UserRequest
{
public:
    virtual ~UserRequest() {}

    bool isTextFieldEditing() { return m_is_text_field_editing; }
private:
    size_t request_number;

    bool m_is_text_field_editing = false;
};

struct UserTextEditRequest : public UserRequest
{
public:
    ~UserTextEditRequest() override {}
private:
    size_t substr_start;
    size_t substr_end;
    std::string text = "";
};

template <typename T>
struct UserGraphicEditRequest : public UserRequest
{
public:
    ~UserGraphicEditRequest() override {}
private:
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
                m_content_field = doc->loadContentFieldFromDocument();
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
                m_content_field = doc->loadContentFieldFromDocument();
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
        std::cout << "Open file - " << path_to_file << "\n";

        /* Создаём файл по пути переданному в качестве аргумента и возвращаем его */

        return m_current_document;
    }

    std::shared_ptr<Document> loadDocument(const std::string& path_to_file)
    {
        std::cout << "Open file - " << path_to_file << "\n";

        /* Проверяем, что файл существует и валидного расширения. Загружаем файл */

        return m_current_document;
    }

    void saveDocument()
    {

    }

private:

    std::shared_ptr<Document> m_current_document;
};

class View
{
public:
    UserRequest* tryGetUserRequest()
    {
        return nullptr;
    }

    void render(const std::shared_ptr<Document>& doc)
    {
        if (doc.checkDocumentIsValid())
        {
            // ...
        }
    }
};

class Controller
{
public:
    Controller() 
    {
        m_view = std::make_unique<View>();
        m_document_loader<DocumentLoader> = std::make_unique();
        m_text_editor<TextEditor<T>> = std::make_unique();
        m_graphic_editor<GraphicEditor<T>> = std::make_unique();
    }

    void openFile(const std::string& path_to_file)
    {
        m_document = m_document_loader->loadDocument(path_to_file);

        m_text_editor->loadTextPart(m_document);
        m_graphic_editor->loadGraphicPart(m_document);
    }

    void createEmptyFile(std::string path_to_file)
    {
        std::shared_ptr<Document> doc = m_document_loader->createEmptyDocument(path_to_file);
        m_view->load(doc);
    }

    void mainLoop()
    {
        while(true)
        {
            std::unique_ptr req = m_view->tryGetUserRequest();

            if (req != nullptr)
            {
                if (req->isTextFieldEditing())
                {
                    //m_text_editor->processTextEditRequest(req);
                }
                else
                {
                    //m_graphic_editor->processGraphicEditRequest(req);
                }
            }

            if (m_document != nullptr)
            {
                m_view->render(m_document);
            }

            updatePeriodic();
        }
    }
private:
    void updatePeriodic() 
    {
        m_document_loader->saveDocument();
    }        

    std::shared_ptr<Document> m_document;

    std::unique_ptr<View> m_view;

    std::unique_ptr<TextEditor<T>> m_text_editor;
    std::unique_ptr<GraphicEditor<T>> m_graphic_editor;
    std::unique_ptr<DocumentLoader> m_document_loader;
};

int main()
{
    std::cout << "This is a document editor template\n";
    return 0;
} 
